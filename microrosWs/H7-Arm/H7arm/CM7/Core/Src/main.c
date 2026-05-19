/* micro-ROS Heartbeat Test over Ethernet
 * Setup stays in main(), runtime behavior is owned by FreeRTOS tasks.
 */

#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/stats.h"
#include "lwip/ip_addr.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "ethernetif.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <uxr/client/transport.h>
#include <rcutils/allocator.h>
#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/float32_multi_array.h>

#include "microros_transports.h"
#include "microros_sim_network.h"
#include "shared_data.h"

extern struct netif gnetif;

#define DEFAULT_DISTANCE_CM 100u
#define HEARTBEAT_PERIOD_MS 500U
#define SENSOR_DEBUG_PERIOD_MS 100U
#define SENSOR_DEBUG_BUFFER_SIZE 256U
#define TELEMETRY_BUFFER_SIZE 160U
#define TIME_SYNC_BUFFER_SIZE 192U
#define JOINT_COUNT 6u
#define JOINT_PUBLISH_PERIOD_MS 20U

static volatile shared_data_t * const sensor_shared_data = SHARED_DATA;
static uint32_t telemetry_cycles_per_us = 0U;
static uint32_t telemetry_cycles_per_ms = 0U;

typedef enum
{
  STATUS_STARTUP_BLINK_GREEN = 0,
  STATUS_RUNNING_SOLID_GREEN,
  STATUS_STARTUP_FATAL_SOLID_RED,
  STATUS_RUNTIME_FAULT_BLINK_RED,
} firmware_status_t;

static uint32_t microros_rand_state = 1u;

static volatile firmware_status_t firmware_status = STATUS_STARTUP_BLINK_GREEN;
static volatile bool healthy_publish_seen = false;
static volatile bool publisher_ready = false;
static volatile uint32_t latest_sensor_distance_cm = DEFAULT_DISTANCE_CM;
static volatile bool sensor_measurement_available = false;
static volatile uint32_t last_seen_cm4_write_seq = 0U;
static volatile uint32_t debug_publish_seq = 0U;

static osThreadId_t statusLedTaskHandle;
static osThreadId_t setupTaskHandle;
static osThreadId_t heartbeatPublisherTaskHandle;
static osThreadId_t sensorDataTaskHandle;
static osThreadId_t sensorDebugTaskHandle;
static osThreadId_t timeSyncTaskHandle;

static rclc_support_t heartbeat_support;
static rcl_node_t heartbeat_node;
static rcl_publisher_t heartbeat_publisher;
static rcl_publisher_t position_publisher;
static rcl_publisher_t sensor_debug_publisher;
static rcl_publisher_t heartbeat_telemetry_publisher;
static rcl_publisher_t position_telemetry_publisher;
static rcl_publisher_t time_sync_echo_publisher;
static rcl_subscription_t time_sync_request_subscription;
static rclc_executor_t time_sync_executor;
static std_msgs__msg__Int32 heartbeat_msg;
static std_msgs__msg__Int32 position_msg;
static std_msgs__msg__String sensor_debug_msg;
static std_msgs__msg__String heartbeat_telemetry_msg;
static std_msgs__msg__String position_telemetry_msg;
static std_msgs__msg__String time_sync_request_msg;
static std_msgs__msg__String time_sync_echo_msg;
static char sensor_debug_buffer[SENSOR_DEBUG_BUFFER_SIZE];
static char heartbeat_telemetry_buffer[TELEMETRY_BUFFER_SIZE];
static char position_telemetry_buffer[TELEMETRY_BUFFER_SIZE];
static char time_sync_request_buffer[TIME_SYNC_BUFFER_SIZE];
static char time_sync_echo_buffer[TIME_SYNC_BUFFER_SIZE];

/* Joint states and commands */
static rcl_publisher_t joint_states_publisher;
static std_msgs__msg__Float32MultiArray joint_states_msg;
static float joint_states_data[64U];
static char joint_states_buffer[256U];
static rcl_subscription_t joint_commands_subscription;
static rclc_executor_t joint_command_executor;
static std_msgs__msg__Float32MultiArray joint_commands_msg;
static char joint_commands_buffer[256U];
static float joint_commanded_positions[JOINT_COUNT];
static float joint_commanded_velocities[JOINT_COUNT];
static float joint_commanded_efforts[JOINT_COUNT];
static uint32_t joint_command_seq;
static volatile bool joint_command_received;
static osThreadId_t jointStatesTaskHandle;
static osThreadId_t jointCommandExecutorTaskHandle;

static bool SetupNetworkingAndMicroRos(void);
static void StartSensorDebugTask(void *argument);
static void StartTimeSyncTask(void *argument);
static void SetRuntimeFault(const char *reason);
static void ResetSharedSensorSnapshot(void);
static void TimeSyncRequestCallback(const void *msg_in);
static void InitHighResolutionClock(void);
static uint64_t GetMonotonicTimeUs(void);
static size_t AppendLiteral(char *buffer, size_t capacity, size_t offset, const char *text);
static size_t AppendUnsignedLong(char *buffer, size_t capacity, size_t offset, unsigned long value);
static size_t AppendSignedLong(char *buffer, size_t capacity, size_t offset, long value);
static size_t AppendUint64(char *buffer, size_t capacity, size_t offset, uint64_t value);
static bool ParseJsonUnsignedLongField(const char *json, const char *field_name, unsigned long *value);
static bool ParseJsonUint64Field(const char *json, const char *field_name, uint64_t *value);
static void ParseJointCommand(const void *msg_in);
static void BuildJointStatesJson(void);
static void JointCommandCallback(const void *msg_in);
static void StartJointStatesTask(void *argument);
static void StartJointCommandExecutorTask(void *argument);

static void InitHighResolutionClock(void)
{
  telemetry_cycles_per_us = SystemCoreClock / 1000000U;
  if(telemetry_cycles_per_us == 0U)
  {
    telemetry_cycles_per_us = 1U;
  }
  telemetry_cycles_per_ms = telemetry_cycles_per_us * 1000U;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR)
  DWT->LAR = 0xC5ACCE55U;
#endif
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint64_t GetMonotonicTimeUs(void)
{
  uint32_t tick_ms_before;
  uint32_t tick_ms_after;
  uint32_t cycle_count;
  uint32_t sub_ms_us;

  do
  {
    tick_ms_before = HAL_GetTick();
    cycle_count = DWT->CYCCNT;
    tick_ms_after = HAL_GetTick();
  } while(tick_ms_before != tick_ms_after);

  if(telemetry_cycles_per_ms == 0U)
  {
    return ((uint64_t)tick_ms_before) * 1000ULL;
  }

  sub_ms_us = (cycle_count % telemetry_cycles_per_ms) / telemetry_cycles_per_us;
  if(sub_ms_us > 999U)
  {
    sub_ms_us = 999U;
  }

  return (((uint64_t)tick_ms_before) * 1000ULL) + (uint64_t)sub_ms_us;
}

static size_t AppendLiteral(char *buffer, size_t capacity, size_t offset, const char *text)
{
  if(buffer == NULL || capacity == 0U || text == NULL)
  {
    return offset;
  }

  while(*text != '\0' && offset + 1U < capacity)
  {
    buffer[offset++] = *text++;
  }
  buffer[offset] = '\0';
  return offset;
}

static size_t AppendUnsignedLong(char *buffer, size_t capacity, size_t offset, unsigned long value)
{
  char temp[16];
  int written;

  if(buffer == NULL || capacity == 0U)
  {
    return offset;
  }

  written = snprintf(temp, sizeof(temp), "%lu", value);
  if(written < 0)
  {
    buffer[offset < capacity ? offset : capacity - 1U] = '\0';
    return offset;
  }

  return AppendLiteral(buffer, capacity, offset, temp);
}

static size_t AppendSignedLong(char *buffer, size_t capacity, size_t offset, long value)
{
  char temp[16];
  int written;

  if(buffer == NULL || capacity == 0U)
  {
    return offset;
  }

  written = snprintf(temp, sizeof(temp), "%ld", value);
  if(written < 0)
  {
    buffer[offset < capacity ? offset : capacity - 1U] = '\0';
    return offset;
  }

  return AppendLiteral(buffer, capacity, offset, temp);
}

static size_t AppendUint64(char *buffer, size_t capacity, size_t offset, uint64_t value)
{
  char digits[21];
  size_t count = 0U;

  if(buffer == NULL || capacity == 0U)
  {
    return offset;
  }

  if(value == 0ULL)
  {
    return AppendLiteral(buffer, capacity, offset, "0");
  }

  while(value > 0ULL && count < sizeof(digits))
  {
    digits[count++] = (char)('0' + (value % 10ULL));
    value /= 10ULL;
  }

  while(count > 0U && offset + 1U < capacity)
  {
    buffer[offset++] = digits[--count];
  }
  buffer[offset] = '\0';
  return offset;
}

static bool ParseJsonUnsignedLongField(const char *json, const char *field_name, unsigned long *value)
{
  const char *field;
  const char *cursor;
  unsigned long parsed = 0UL;

  if(json == NULL || field_name == NULL || value == NULL)
  {
    return false;
  }

  field = strstr(json, field_name);
  if(field == NULL)
  {
    return false;
  }

  cursor = field + strlen(field_name);
  if(*cursor != ':')
  {
    return false;
  }
  cursor++;

  if(*cursor < '0' || *cursor > '9')
  {
    return false;
  }

  while(*cursor >= '0' && *cursor <= '9')
  {
    parsed = (parsed * 10UL) + (unsigned long)(*cursor - '0');
    cursor++;
  }

  *value = parsed;
  return true;
}

static bool ParseJsonUint64Field(const char *json, const char *field_name, uint64_t *value)
{
  const char *field;
  const char *cursor;
  uint64_t parsed = 0ULL;

  if(json == NULL || field_name == NULL || value == NULL)
  {
    return false;
  }

  field = strstr(json, field_name);
  if(field == NULL)
  {
    return false;
  }

  cursor = field + strlen(field_name);
  if(*cursor != ':')
  {
    return false;
  }
  cursor++;

  if(*cursor < '0' || *cursor > '9')
  {
    return false;
  }

  while(*cursor >= '0' && *cursor <= '9')
  {
    parsed = (parsed * 10ULL) + (uint64_t)(*cursor - '0');
    cursor++;
  }

  *value = parsed;
  return true;
}

static void ParseJointCommand(const void *msg_in)
{
  const std_msgs__msg__Float32MultiArray *incoming = (const std_msgs__msg__Float32MultiArray *)msg_in;
  size_t i;

  if(incoming == NULL || incoming->data.data == NULL)
  {
    return;
  }

  if(incoming->data.size < JOINT_COUNT)
  {
    return;
  }

  for(i = 0U; i < JOINT_COUNT; i++)
  {
    joint_commanded_positions[i] = incoming->data.data[i];
  }

  if(incoming->data.size >= JOINT_COUNT * 2U)
  {
    for(i = 0U; i < JOINT_COUNT; i++)
    {
      joint_commanded_velocities[i] = incoming->data.data[JOINT_COUNT + i];
    }
  }
  else
  {
    for(i = 0U; i < JOINT_COUNT; i++)
    {
      joint_commanded_velocities[i] = 0.0f;
    }
  }

  if(incoming->data.size >= JOINT_COUNT * 3U)
  {
    for(i = 0U; i < JOINT_COUNT; i++)
    {
      joint_commanded_efforts[i] = incoming->data.data[JOINT_COUNT * 2U + i];
    }
  }
  else
  {
    for(i = 0U; i < JOINT_COUNT; i++)
    {
      joint_commanded_efforts[i] = 0.0f;
    }
  }

  joint_command_seq++;
  joint_command_received = true;

  /* Relay commanded positions to shared SRAM4 for CM4 motor driver */
  for(i = 0U; i < JOINT_COUNT && i < SHARED_JOINT_COUNT; i++)
  {
    sensor_shared_data->joint_cmd_positions[i] = joint_commanded_positions[i];
  }
  sensor_shared_data->joint_cmd_seq = joint_command_seq;
  SCB_CleanDCache_by_Addr((uint32_t *)&sensor_shared_data->joint_cmd_positions[0],
                           sizeof(sensor_shared_data->joint_cmd_positions) +
                           sizeof(sensor_shared_data->joint_cmd_seq));
  __DSB();
}

static void BuildJointStatesJson(void)
{
  size_t length = 0U;
  size_t i;

  joint_states_buffer[0] = '\0';
  length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "{\"positions\":[");
  for(i = 0U; i < JOINT_COUNT; i++)
  {
    if(i > 0U)
    {
      length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, ",");
    }
    length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "\"");
    {
      char temp[32];
      int written = snprintf(temp, sizeof(temp), "%.6f", joint_commanded_positions[i]);
      if(written > 0 && (size_t)written < sizeof(temp))
      {
        length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, temp);
      }
    }
  }
  length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "],\"velocities\":[");
  for(i = 0U; i < JOINT_COUNT; i++)
  {
    if(i > 0U)
    {
      length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, ",");
    }
    length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "\"");
    {
      char temp[32];
      int written = snprintf(temp, sizeof(temp), "%.6f", joint_commanded_velocities[i]);
      if(written > 0 && (size_t)written < sizeof(temp))
      {
        length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, temp);
      }
    }
  }
  length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "],\"efforts\":[");
  for(i = 0U; i < JOINT_COUNT; i++)
  {
    if(i > 0U)
    {
      length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, ",");
    }
    length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "\"");
    {
      char temp[32];
      int written = snprintf(temp, sizeof(temp), "%.6f", joint_commanded_efforts[i]);
      if(written > 0 && (size_t)written < sizeof(temp))
      {
        length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, temp);
      }
    }
  }
  length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "],\"seq\":");
  length = AppendUnsignedLong(joint_states_buffer, sizeof(joint_states_buffer), length, (unsigned long)joint_command_seq);
  length = AppendLiteral(joint_states_buffer, sizeof(joint_states_buffer), length, "}");

  /* Also fill the Float32MultiArray data with actual positions from CM4 */
  SCB_InvalidateDCache_by_Addr((uint32_t *)&sensor_shared_data->joint_act_positions[0],
                                sizeof(sensor_shared_data->joint_act_positions));
  joint_states_msg.data.size = JOINT_COUNT * 3u;
  for(i = 0U; i < JOINT_COUNT; i++)
  {
    /* Use actual positions from CM4 if motor driver is ready, else echo commanded */
    if(sensor_shared_data->motor_ready)
    {
      joint_states_data[i] = (i < SHARED_JOINT_COUNT) ?
          sensor_shared_data->joint_act_positions[i] : 0.0f;
    }
    else
    {
      joint_states_data[i] = joint_commanded_positions[i];
    }
    joint_states_data[JOINT_COUNT + i] = joint_commanded_velocities[i];
    joint_states_data[JOINT_COUNT * 2u + i] = joint_commanded_efforts[i];
  }
}

static void JointCommandCallback(const void *msg_in)
{
  ParseJointCommand(msg_in);
}

static void StartJointStatesTask(void *argument)
{
  (void)argument;
  uint64_t next_release_us = 0ULL;
  const uint64_t publish_period_us = ((uint64_t)JOINT_PUBLISH_PERIOD_MS) * 1000ULL;

  for(;;)
  {
      if(!publisher_ready)
          {
            osDelay(100);
            continue;
          }

    uint64_t now_us;
    uint64_t remaining_us;

    now_us = GetMonotonicTimeUs();
    if(next_release_us == 0ULL)
    {
      next_release_us = now_us;
    }

    next_release_us += publish_period_us;

    while(true)
    {
      now_us = GetMonotonicTimeUs();
      if(now_us >= next_release_us)
      {
        break;
      }
      remaining_us = next_release_us - now_us;
      if(remaining_us > 5000ULL)
      {
        osDelay(5);
      }
      else if(remaining_us > 1000ULL)
      {
        osDelay(1);
      }
      else
      {
        osThreadYield();
      }
    }

    BuildJointStatesJson();
    if(rcl_publish(&joint_states_publisher, &joint_states_msg, NULL) != RCL_RET_OK && healthy_publish_seen)
    {
      SetRuntimeFault("joint-states-publish");
    }
  }
}

static void StartJointCommandExecutorTask(void *argument)
{
  (void)argument;

  for(;;)
  {
    if(!publisher_ready)
    {
      osDelay(100);
      continue;
    }

    if(rclc_executor_spin_some(&joint_command_executor, RCL_MS_TO_NS(20)) != RCL_RET_OK &&
       healthy_publish_seen)
    {
      SetRuntimeFault("joint-command-spin");
    }
    osDelay(20);
  }
}

static bool PublishTelemetrySample(
  rcl_publisher_t *publisher,
  std_msgs__msg__String *message,
  char *buffer,
  size_t buffer_size,
  const char *topic_name,
  uint32_t sequence,
  int32_t value)
{
  size_t length = 0U;
  const uint64_t publish_us = GetMonotonicTimeUs();

  if(publisher == NULL || message == NULL || buffer == NULL || buffer_size == 0U || topic_name == NULL)
  {
    message->data.size = 0U;
    buffer[0] = '\0';
    return false;
  }

  buffer[0] = '\0';
  length = AppendLiteral(buffer, buffer_size, length, "{\"topic\":\"");
  length = AppendLiteral(buffer, buffer_size, length, topic_name);
  length = AppendLiteral(buffer, buffer_size, length, "\",\"seq\":");
  length = AppendUnsignedLong(buffer, buffer_size, length, (unsigned long)sequence);
  length = AppendLiteral(buffer, buffer_size, length, ",\"value\":");
  length = AppendSignedLong(buffer, buffer_size, length, (long)value);
  length = AppendLiteral(buffer, buffer_size, length, ",\"publish_us\":");
  length = AppendUint64(buffer, buffer_size, length, publish_us);
  length = AppendLiteral(buffer, buffer_size, length, "}");

  message->data.size = length;

  return rcl_publish(publisher, message, NULL) == RCL_RET_OK;
}

static void WaitUntilNextHeartbeatPeriod(uint64_t *next_release_us)
{
  const uint64_t heartbeat_period_us = ((uint64_t)HEARTBEAT_PERIOD_MS) * 1000ULL;
  uint64_t now_us;
  uint64_t remaining_us;

  if(next_release_us == NULL)
  {
    osDelay(HEARTBEAT_PERIOD_MS);
    return;
  }

  now_us = GetMonotonicTimeUs();
  if(*next_release_us == 0ULL)
  {
    *next_release_us = now_us;
  }

  *next_release_us += heartbeat_period_us;

  while(true)
  {
    now_us = GetMonotonicTimeUs();
    if(now_us >= *next_release_us)
    {
      break;
    }

    remaining_us = *next_release_us - now_us;
    if(remaining_us > 5000ULL)
    {
      osDelay(5);
    }
    else if(remaining_us > 1000ULL)
    {
      osDelay(1);
    }
    else
    {
      osThreadYield();
    }
  }
}

static void TimeSyncRequestCallback(const void *msg_in)
{
  const std_msgs__msg__String *incoming = (const std_msgs__msg__String *)msg_in;
  unsigned long seq = 0UL;
  uint64_t host_send_us = 0ULL;
  const uint64_t cm7_recv_us = GetMonotonicTimeUs();
  const uint64_t cm7_send_us = GetMonotonicTimeUs();
  size_t length = 0U;

  if(incoming == NULL || incoming->data.data == NULL)
  {
    return;
  }

  if(!ParseJsonUnsignedLongField(incoming->data.data, "\"seq\"", &seq) ||
     !ParseJsonUint64Field(incoming->data.data, "\"host_send_us\"", &host_send_us))
  {
    return;
  }

  time_sync_echo_buffer[0] = '\0';
  length = AppendLiteral(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, "{\"seq\":");
  length = AppendUnsignedLong(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, seq);
  length = AppendLiteral(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, ",\"host_send_us\":");
  length = AppendUint64(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, host_send_us);
  length = AppendLiteral(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, ",\"cm7_recv_us\":");
  length = AppendUint64(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, cm7_recv_us);
  length = AppendLiteral(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, ",\"cm7_send_us\":");
  length = AppendUint64(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, cm7_send_us);
  length = AppendLiteral(time_sync_echo_buffer, sizeof(time_sync_echo_buffer), length, "}");
  time_sync_echo_msg.data.size = length;

  if(rcl_publish(&time_sync_echo_publisher, &time_sync_echo_msg, NULL) != RCL_RET_OK &&
     healthy_publish_seen)
  {
    SetRuntimeFault("time-sync-echo-publish");
  }
}

static void StartSensorDataTask(void *argument)
{
  (void)argument;

  for(;;)
  {
    /* Shared SRAM4 is written by CM4, so invalidate any cached copy before reading. */
    SCB_InvalidateDCache_by_Addr((void *)sensor_shared_data, (int32_t)sizeof(*sensor_shared_data));
    __DSB();
    if(sensor_shared_data->data_ready)
    {
      latest_sensor_distance_cm = sensor_shared_data->distance_cm;
      sensor_measurement_available = true;
      last_seen_cm4_write_seq = sensor_shared_data->cm4_write_seq;
      sensor_shared_data->data_ready = 0U;
      SCB_CleanDCache_by_Addr((uint32_t *)sensor_shared_data, (int32_t)sizeof(*sensor_shared_data));
      __DSB();
    }

    osDelay(100);
  }
}

static void StartSensorDebugTask(void *argument)
{
  (void)argument;

  for(;;)
  {
    shared_data_t shared_snapshot;
    int written;

    if(!publisher_ready)
    {
      osDelay(SENSOR_DEBUG_PERIOD_MS);
      continue;
    }

    SCB_InvalidateDCache_by_Addr((void *)sensor_shared_data, (int32_t)sizeof(*sensor_shared_data));
    __DSB();
    shared_snapshot = *sensor_shared_data;

    written = snprintf(
      sensor_debug_buffer,
      sizeof(sensor_debug_buffer),
      "hb=%ld dbg=%lu fw=%d avail=%d latest=%lu shared_dist=%lu ready=%lu cm4_seq=%lu "
      "last_seen_cm4_seq=%lu echo_ok=%lu echo_ticks=%lu wait_to=%lu pulse_to=%lu valid=%lu",
      (long)heartbeat_msg.data,
      (unsigned long)debug_publish_seq,
      (int)firmware_status,
      sensor_measurement_available ? 1 : 0,
      (unsigned long)latest_sensor_distance_cm,
      (unsigned long)shared_snapshot.distance_cm,
      (unsigned long)shared_snapshot.data_ready,
      (unsigned long)shared_snapshot.cm4_write_seq,
      (unsigned long)last_seen_cm4_write_seq,
      (unsigned long)shared_snapshot.cm4_last_echo_ok,
      (unsigned long)shared_snapshot.cm4_last_echo_ticks,
      (unsigned long)shared_snapshot.cm4_last_wait_timeout,
      (unsigned long)shared_snapshot.cm4_last_pulse_timeout,
      (unsigned long)shared_snapshot.cm4_last_measurement_valid);

    if(written < 0)
    {
      sensor_debug_buffer[0] = '\0';
      sensor_debug_msg.data.size = 0U;
    }
    else if((size_t)written >= sizeof(sensor_debug_buffer))
    {
      sensor_debug_msg.data.size = sizeof(sensor_debug_buffer) - 1U;
    }
    else
    {
      sensor_debug_msg.data.size = (size_t)written;
    }

    if(rcl_publish(&sensor_debug_publisher, &sensor_debug_msg, NULL) != RCL_RET_OK &&
       healthy_publish_seen)
    {
      SetRuntimeFault("sensor-debug-publish");
    }

    debug_publish_seq++;
    osDelay(SENSOR_DEBUG_PERIOD_MS);
  }
}

static void StartTimeSyncTask(void *argument)
{
  (void)argument;

  for(;;)
  {
    if(!publisher_ready)
    {
      osDelay(100);
      continue;
    }

    if(rclc_executor_spin_some(&time_sync_executor, RCL_MS_TO_NS(20)) != RCL_RET_OK &&
       healthy_publish_seen)
    {
      SetRuntimeFault("time-sync-spin");
    }
    osDelay(20);
  }
}

static void SetGreenLed(bool enabled)
{
  if(enabled)
  {
    GPIOB->BSRR = (1u << 0);
  }
  else
  {
    GPIOB->BSRR = (1u << 16);
  }
}

static void SetRedLed(bool enabled)
{
  if(enabled)
  {
    GPIOB->BSRR = (1u << 14);
  }
  else
  {
    GPIOB->BSRR = (1u << 30);
  }
}

static void SetFirmwareStatus(firmware_status_t status)
{
  firmware_status = status;
}

static void SetStartupFatalError(const char *reason)
{
  printf("CM7: startup-fatal=%s\r\n", reason);
  SetFirmwareStatus(STATUS_STARTUP_FATAL_SOLID_RED);
}

static void SetRuntimeFault(const char *reason)
{
  if(healthy_publish_seen)
  {
    printf("CM7: runtime-fault=%s\r\n", reason);
    SetFirmwareStatus(STATUS_RUNTIME_FAULT_BLINK_RED);
  }
}

static void StartStatusLedTask(void *argument)
{
  bool phase = false;
  (void)argument;

  for(;;)
  {
    switch(firmware_status)
    {
      case STATUS_STARTUP_BLINK_GREEN:
        SetRedLed(false);
        SetGreenLed(phase);
        phase = !phase;
        osDelay(250);
        break;
      case STATUS_RUNNING_SOLID_GREEN:
        SetRedLed(false);
        SetGreenLed(true);
        phase = false;
        osDelay(250);
        break;
      case STATUS_STARTUP_FATAL_SOLID_RED:
        SetGreenLed(false);
        SetRedLed(true);
        phase = false;
        osDelay(250);
        break;
      case STATUS_RUNTIME_FAULT_BLINK_RED:
        SetGreenLed(false);
        SetRedLed(phase);
        phase = !phase;
        osDelay(250);
        break;
      default:
        SetGreenLed(false);
        SetRedLed(false);
        osDelay(250);
        break;
    }
  }
}

static void StartHeartbeatPublisherTask(void *argument)
{
  int heartbeat_failure_count = 0;
  int position_failure_count = 0;
  uint32_t position_publish_sequence = 0U;
  uint64_t next_release_us = 0ULL;
  (void)argument;

  printf("CM7: publisher-task-start\r\n");

  for(;;)
  {
    const int32_t measured_distance_cm = sensor_measurement_available
      ? (int32_t)latest_sensor_distance_cm
      : (int32_t)DEFAULT_DISTANCE_CM;
    const uint32_t heartbeat_publish_sequence = (uint32_t)heartbeat_msg.data;
    rcl_ret_t ret = rcl_publish(&heartbeat_publisher, &heartbeat_msg, NULL);

    if(ret == RCL_RET_OK)
    {
      if(!PublishTelemetrySample(
           &heartbeat_telemetry_publisher,
           &heartbeat_telemetry_msg,
           heartbeat_telemetry_buffer,
           sizeof(heartbeat_telemetry_buffer),
           "heartbeat",
           heartbeat_publish_sequence,
           heartbeat_msg.data) &&
         healthy_publish_seen)
      {
        SetRuntimeFault("heartbeat-telemetry-publish");
      }
      printf("CM7: publish-ok seq=%ld\r\n", (long)heartbeat_msg.data);
      heartbeat_msg.data++;
      heartbeat_failure_count = 0;

      if(!healthy_publish_seen)
      {
        healthy_publish_seen = true;
        SetFirmwareStatus(STATUS_RUNNING_SOLID_GREEN);
      }
    }
    else
    {
      heartbeat_failure_count++;
      printf("CM7: publish-failed ret=%ld count=%d\r\n", (long)ret, heartbeat_failure_count);

      if(healthy_publish_seen)
      {
        SetRuntimeFault("publish");
      }
      else if(heartbeat_failure_count >= 10)
      {
        SetStartupFatalError("publish-before-healthy");
        while(1)
        {
          osDelay(1000);
        }
      }
      else
      {
        SetFirmwareStatus(STATUS_STARTUP_BLINK_GREEN);
      }
    }

    position_msg.data = measured_distance_cm;
    ret = rcl_publish(&position_publisher, &position_msg, NULL);
    if(ret == RCL_RET_OK)
    {
      if(!PublishTelemetrySample(
           &position_telemetry_publisher,
           &position_telemetry_msg,
           position_telemetry_buffer,
           sizeof(position_telemetry_buffer),
           "measured_position",
           position_publish_sequence,
           position_msg.data) &&
         healthy_publish_seen)
      {
        SetRuntimeFault("position-telemetry-publish");
      }
      position_publish_sequence++;
      position_failure_count = 0;
    }
    else
    {
      position_failure_count++;
      printf("CM7: position-publish-failed ret=%ld count=%d\r\n", (long)ret, position_failure_count);

      if(healthy_publish_seen)
      {
        SetRuntimeFault("position-publish");
      }
    }

    WaitUntilNextHeartbeatPeriod(&next_release_us);
  }
}

static void StartSetupTask(void *argument)
{
  (void)argument;

  printf("CM7: task-start\r\n");

  if(!SetupNetworkingAndMicroRos())
  {
    osThreadExit();
  }

  if(heartbeatPublisherTaskHandle == NULL)
  {
    const osThreadAttr_t heartbeat_publisher_task_attributes = {
      .name = "HeartbeatPub",
      .stack_size = 4096,
      .priority = osPriorityNormal,
    };

    heartbeatPublisherTaskHandle = osThreadNew(
      StartHeartbeatPublisherTask,
      NULL,
      &heartbeat_publisher_task_attributes);
    if(heartbeatPublisherTaskHandle == NULL)
    {
      SetStartupFatalError("publisher-task");
      osThreadExit();
    }
  }

  osThreadExit();
}

static void ethernet_link_status_updated(struct netif *netif)
{
  const bool link_is_up = netif_is_link_up(netif);
  printf("CM7: link-%s\r\n", link_is_up ? "up" : "down");

  if(!link_is_up && healthy_publish_seen)
  {
    SetRuntimeFault("link-down");
  }
}

static bool SetupNetworkingAndMicroRos(void)
{
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;
  rcl_allocator_t freeRTOS_allocator;
  rcl_allocator_t allocator;
  rmw_ret_t transport_ret;
  rcl_ret_t support_ret;
  rcl_ret_t node_ret;
  rcl_ret_t pub_ret;
  rcl_ret_t position_pub_ret;
  rcl_ret_t sensor_debug_pub_ret;
  rcl_ret_t heartbeat_telemetry_pub_ret;
  rcl_ret_t position_telemetry_pub_ret;
  rcl_ret_t time_sync_echo_pub_ret;
  rcl_ret_t time_sync_request_sub_ret;
  rcl_ret_t time_sync_executor_ret;
  rcl_ret_t joint_states_pub_ret;
  rcl_ret_t joint_commands_sub_ret;
  rcl_ret_t joint_command_executor_ret;
  osThreadAttr_t eth_link_attributes = {
    .name = "EthLink",
    .stack_size = 1024,
    .priority = osPriorityBelowNormal,
  };

  printf("CM7: setup-start\r\n");

  tcpip_init(NULL, NULL);
  printf("CM7: lwip-core-init-ok\r\n");

  MX_ETH_Init();
  printf("CM7: eth-init-ok\r\n");

  IP4_ADDR(&ipaddr,
           MICROROS_DEVICE_IP_A,
           MICROROS_DEVICE_IP_B,
           MICROROS_DEVICE_IP_C,
           MICROROS_DEVICE_IP_D);
  IP4_ADDR(&netmask,
           MICROROS_NETMASK_A,
           MICROROS_NETMASK_B,
           MICROROS_NETMASK_C,
           MICROROS_NETMASK_D);
  IP4_ADDR(&gw,
           MICROROS_GATEWAY_IP_A,
           MICROROS_GATEWAY_IP_B,
           MICROROS_GATEWAY_IP_C,
           MICROROS_GATEWAY_IP_D);

  if(netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input) == NULL)
  {
    SetStartupFatalError("netif-add");
    return false;
  }
  netif_set_default(&gnetif);
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);

  if(osThreadNew(ethernet_link_thread, &gnetif, &eth_link_attributes) == NULL)
  {
    SetStartupFatalError("eth-link-thread");
    return false;
  }

  printf("CM7: eth-link-thread-created\r\n");
  printf("CM7: netif-added ip=%d.%d.%d.%d agent=%s\r\n",
         MICROROS_DEVICE_IP_A,
         MICROROS_DEVICE_IP_B,
         MICROROS_DEVICE_IP_C,
         MICROROS_DEVICE_IP_D,
         MICROROS_AGENT_IP);
  printf("CM7: link-%s\r\n", netif_is_link_up(&gnetif) ? "up" : "down");

  freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
  freeRTOS_allocator.allocate = microros_allocate;
  freeRTOS_allocator.deallocate = microros_deallocate;
  freeRTOS_allocator.reallocate = microros_reallocate;
  freeRTOS_allocator.zero_allocate = microros_zero_allocate;
  if(!rcutils_set_default_allocator(&freeRTOS_allocator))
  {
    SetStartupFatalError("allocator");
    return false;
  }
  printf("CM7: allocator-configured=1\r\n");

  transport_ret = rmw_uros_set_custom_transport(
    false,
    MICROROS_AGENT_IP,
    cubemx_transport_open,
    cubemx_transport_close,
    cubemx_transport_write,
    cubemx_transport_read);
  printf("CM7: transport-configured ret=%ld ok=%d\r\n",
         (long)transport_ret,
         transport_ret == RMW_RET_OK ? 1 : 0);
  if(transport_ret != RMW_RET_OK)
  {
    SetStartupFatalError("transport");
    return false;
  }

  allocator = rcl_get_default_allocator();
  heartbeat_support = (rclc_support_t){0};
  do
  {
    heartbeat_support = (rclc_support_t){0};
    support_ret = rclc_support_init(&heartbeat_support, 0, NULL, &allocator);
    printf("CM7: support-init=%ld\r\n", (long)support_ret);
    if(support_ret == RCL_RET_OK)
    {
      break;
    }

    printf("CM7: support-waiting-for-agent\r\n");
    SetFirmwareStatus(STATUS_STARTUP_BLINK_GREEN);
    osDelay(2000);
  } while(true);

  heartbeat_node = rcl_get_zero_initialized_node();
  node_ret = rclc_node_init_default(&heartbeat_node, "heartbeat_test", "", &heartbeat_support);
  printf("CM7: node-init=%ld\r\n", (long)node_ret);
  if(node_ret != RCL_RET_OK)
  {
    SetStartupFatalError("node");
    return false;
  }

  heartbeat_publisher = rcl_get_zero_initialized_publisher();
  pub_ret = rclc_publisher_init_default(
    &heartbeat_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "heartbeat");
  printf("CM7: publisher-init=%ld\r\n", (long)pub_ret);
  if(pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("publisher");
    return false;
  }

  position_publisher = rcl_get_zero_initialized_publisher();
  position_pub_ret = rclc_publisher_init_default(
    &position_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "measured_position");
  printf("CM7: position-publisher-init=%ld\r\n", (long)position_pub_ret);
  if(position_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("position-publisher");
    return false;
  }

  sensor_debug_publisher = rcl_get_zero_initialized_publisher();
  sensor_debug_pub_ret = rclc_publisher_init_default(
    &sensor_debug_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "sensor_debug");
  printf("CM7: sensor-debug-publisher-init=%ld\r\n", (long)sensor_debug_pub_ret);
  if(sensor_debug_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("sensor-debug-publisher");
    return false;
  }

  heartbeat_telemetry_publisher = rcl_get_zero_initialized_publisher();
  heartbeat_telemetry_pub_ret = rclc_publisher_init_default(
    &heartbeat_telemetry_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "heartbeat_telemetry");
  printf("CM7: heartbeat-telemetry-publisher-init=%ld\r\n", (long)heartbeat_telemetry_pub_ret);
  if(heartbeat_telemetry_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("heartbeat-telemetry-publisher");
    return false;
  }

  position_telemetry_publisher = rcl_get_zero_initialized_publisher();
  position_telemetry_pub_ret = rclc_publisher_init_default(
    &position_telemetry_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "measured_position_telemetry");
  printf("CM7: position-telemetry-publisher-init=%ld\r\n", (long)position_telemetry_pub_ret);
  if(position_telemetry_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("position-telemetry-publisher");
    return false;
  }

  time_sync_echo_publisher = rcl_get_zero_initialized_publisher();
  time_sync_echo_pub_ret = rclc_publisher_init_default(
    &time_sync_echo_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "time_sync_echo");
  printf("CM7: time-sync-echo-publisher-init=%ld\r\n", (long)time_sync_echo_pub_ret);
  if(time_sync_echo_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("time-sync-echo-publisher");
    return false;
  }

  time_sync_request_subscription = rcl_get_zero_initialized_subscription();
  time_sync_request_sub_ret = rclc_subscription_init_default(
    &time_sync_request_subscription,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "time_sync_request");
  printf("CM7: time-sync-request-subscription-init=%ld\r\n", (long)time_sync_request_sub_ret);
  if(time_sync_request_sub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("time-sync-request-subscription");
    return false;
  }

  time_sync_executor = rclc_executor_get_zero_initialized_executor();
  time_sync_executor_ret = rclc_executor_init(&time_sync_executor, &heartbeat_support.context, 1, &allocator);
  printf("CM7: time-sync-executor-init=%ld\r\n", (long)time_sync_executor_ret);
  if(time_sync_executor_ret != RCL_RET_OK)
  {
    SetStartupFatalError("time-sync-executor");
    return false;
  }

  time_sync_request_msg.data.data = time_sync_request_buffer;
  time_sync_request_msg.data.size = 0U;
  time_sync_request_msg.data.capacity = sizeof(time_sync_request_buffer);
  time_sync_request_buffer[0] = '\0';
  time_sync_executor_ret = rclc_executor_add_subscription(
    &time_sync_executor,
    &time_sync_request_subscription,
    &time_sync_request_msg,
    &TimeSyncRequestCallback,
    ON_NEW_DATA);
  printf("CM7: time-sync-executor-add-subscription=%ld\r\n", (long)time_sync_executor_ret);
  if(time_sync_executor_ret != RCL_RET_OK)
  {
    SetStartupFatalError("time-sync-executor-subscription");
    return false;
  }

  joint_states_publisher = rcl_get_zero_initialized_publisher();
  joint_states_pub_ret = rclc_publisher_init_default(
    &joint_states_publisher,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "joint_states");
  printf("CM7: joint-states-publisher-init=%ld\r\n", (long)joint_states_pub_ret);
  if(joint_states_pub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("joint-states-publisher");
    return false;
  }

  joint_commands_subscription = rcl_get_zero_initialized_subscription();
  joint_commands_sub_ret = rclc_subscription_init_default(
    &joint_commands_subscription,
    &heartbeat_node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "joint_commands");
  printf("CM7: joint-commands-subscription-init=%ld\r\n", (long)joint_commands_sub_ret);
  if(joint_commands_sub_ret != RCL_RET_OK)
  {
    SetStartupFatalError("joint-commands-subscription");
    return false;
  }

  joint_command_executor = rclc_executor_get_zero_initialized_executor();
  joint_command_executor_ret = rclc_executor_init(&joint_command_executor, &heartbeat_support.context, 1, &allocator);
  printf("CM7: joint-command-executor-init=%ld\r\n", (long)joint_command_executor_ret);
  if(joint_command_executor_ret != RCL_RET_OK)
  {
    SetStartupFatalError("joint-command-executor");
    return false;
  }

  joint_commands_msg.data.data = (float *)joint_commands_buffer;
  joint_commands_msg.data.size = 0U;
  joint_commands_msg.data.capacity = sizeof(joint_commands_buffer) / sizeof(float);
  memset(joint_commands_buffer, 0, sizeof(joint_commands_buffer));
  joint_command_executor_ret = rclc_executor_add_subscription(
    &joint_command_executor,
    &joint_commands_subscription,
    &joint_commands_msg,
    &JointCommandCallback,
    ON_NEW_DATA);
  printf("CM7: joint-command-executor-add-subscription=%ld\r\n", (long)joint_command_executor_ret);
  if(joint_command_executor_ret != RCL_RET_OK)
  {
    SetStartupFatalError("joint-command-executor-subscription");
    return false;
  }

  heartbeat_msg.data = 0;
  position_msg.data = (int32_t)DEFAULT_DISTANCE_CM;
  sensor_debug_msg.data.data = sensor_debug_buffer;
  sensor_debug_msg.data.size = 0U;
  sensor_debug_msg.data.capacity = sizeof(sensor_debug_buffer);
  sensor_debug_buffer[0] = '\0';
  heartbeat_telemetry_msg.data.data = heartbeat_telemetry_buffer;
  heartbeat_telemetry_msg.data.size = 0U;
  heartbeat_telemetry_msg.data.capacity = sizeof(heartbeat_telemetry_buffer);
  heartbeat_telemetry_buffer[0] = '\0';
  position_telemetry_msg.data.data = position_telemetry_buffer;
  position_telemetry_msg.data.size = 0U;
  position_telemetry_msg.data.capacity = sizeof(position_telemetry_buffer);
  position_telemetry_buffer[0] = '\0';
  time_sync_echo_msg.data.data = time_sync_echo_buffer;
  time_sync_echo_msg.data.size = 0U;
  time_sync_echo_msg.data.capacity = sizeof(time_sync_echo_buffer);
  time_sync_echo_buffer[0] = '\0';
  joint_states_msg.data.data = joint_states_data;
  joint_states_msg.data.size = 0U;
  joint_states_msg.data.capacity = sizeof(joint_states_data) / sizeof(float);
  memset(joint_states_buffer, 0, sizeof(joint_states_buffer));
  memset(joint_commanded_positions, 0, sizeof(joint_commanded_positions));
  memset(joint_commanded_velocities, 0, sizeof(joint_commanded_velocities));
  memset(joint_commanded_efforts, 0, sizeof(joint_commanded_efforts));
  joint_command_seq = 0U;
  joint_command_received = false;
  publisher_ready = true;
  printf("CM7: setup-complete\r\n");
  return true;
}

void srand(unsigned int seed)
{
  microros_rand_state = seed != 0u ? seed : 1u;
}

int rand(void)
{
  microros_rand_state = (microros_rand_state * 1103515245u) + 12345u;
  return (int)((microros_rand_state >> 16) & 0x7FFFu);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 28;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 1024;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                              | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  InitHighResolutionClock();
}

void MX_FREERTOS_Init(void)
{
  const osThreadAttr_t status_led_task_attributes = {
    .name = "StatusLed",
    .stack_size = 1024,
    .priority = osPriorityLow,
  };
  const osThreadAttr_t setup_task_attributes = {
    .name = "SetupTask",
    .stack_size = 6144,
    .priority = osPriorityAboveNormal,
  };

  statusLedTaskHandle = osThreadNew(StartStatusLedTask, NULL, &status_led_task_attributes);
  if(statusLedTaskHandle == NULL)
  {
    SetGreenLed(false);
    SetRedLed(true);
    while(1) {}
  }

  setupTaskHandle = osThreadNew(StartSetupTask, NULL, &setup_task_attributes);
  if(setupTaskHandle == NULL)
  {
    SetStartupFatalError("setup-task");
  }

  {
    const osThreadAttr_t sensor_data_task_attributes = {
      .name = "SensorData",
      .stack_size = 1024,
      .priority = osPriorityNormal,
    };

    sensorDataTaskHandle = osThreadNew(StartSensorDataTask, NULL, &sensor_data_task_attributes);
    if(sensorDataTaskHandle == NULL)
    {
      SetStartupFatalError("sensor-data-task");
    }
  }

  {
    const osThreadAttr_t sensor_debug_task_attributes = {
      .name = "SensorDebug",
      .stack_size = 4096,
      .priority = osPriorityBelowNormal,
    };

    sensorDebugTaskHandle = osThreadNew(StartSensorDebugTask, NULL, &sensor_debug_task_attributes);
    if(sensorDebugTaskHandle == NULL)
    {
      SetStartupFatalError("sensor-debug-task");
    }
  }

   {
     const osThreadAttr_t time_sync_task_attributes = {
       .name = "TimeSync",
       .stack_size = 4096,
       .priority = osPriorityNormal,
     };

     timeSyncTaskHandle = osThreadNew(StartTimeSyncTask, NULL, &time_sync_task_attributes);
     if(timeSyncTaskHandle == NULL)
     {
       SetStartupFatalError("time-sync-task");
     }
   }

   {
     const osThreadAttr_t joint_states_task_attributes = {
       .name = "JointStates",
       .stack_size = 4096,
       .priority = osPriorityNormal,
     };

     jointStatesTaskHandle = osThreadNew(StartJointStatesTask, NULL, &joint_states_task_attributes);
     if(jointStatesTaskHandle == NULL)
     {
       SetStartupFatalError("joint-states-task");
     }
   }

   {
     const osThreadAttr_t joint_command_executor_task_attributes = {
       .name = "JointCmdExec",
       .stack_size = 4096,
       .priority = osPriorityNormal,
     };

     jointCommandExecutorTaskHandle = osThreadNew(StartJointCommandExecutorTask, NULL, &joint_command_executor_task_attributes);
     if(jointCommandExecutorTaskHandle == NULL)
     {
       SetStartupFatalError("joint-command-executor-task");
     }
   }
}

int main(void)
{
  RCC->AHB4ENR |= (1u << 1);
  for(volatile int i = 0; i < 10000; i++) {}

  GPIOB->MODER &= ~(3u << 0);
  GPIOB->MODER |= (1u << 0);
  GPIOB->MODER &= ~(3u << 28);
  GPIOB->MODER |= (1u << 28);

  SetGreenLed(true);
  SetRedLed(false);

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;
  ResetSharedSensorSnapshot();
  Debug_USART3_Init();
  Debug_USART3_Print("CM7: boot\r\n");
  printf("CM7: hal-clock-init-ok\r\n");

  osKernelInitialize();
  printf("CM7: kernel-initialize-ok\r\n");

  MX_FREERTOS_Init();
  printf("CM7: freertos-init-ok\r\n");
  osKernelStart();

  while(1) {}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  (void)htim;
}

void Error_Handler(void)
{
  if(healthy_publish_seen)
  {
    SetGreenLed(false);
    while(1)
    {
      SetRedLed(true);
      for(volatile int i = 0; i < 1500000; i++) {}
      SetRedLed(false);
      for(volatile int i = 0; i < 1500000; i++) {}
    }
  }

  SetGreenLed(false);
  SetRedLed(true);
  while(1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  Error_Handler();
}
#endif

static void ResetSharedSensorSnapshot(void)
{
  memset((void *)sensor_shared_data, 0, sizeof(*sensor_shared_data));
  latest_sensor_distance_cm = DEFAULT_DISTANCE_CM;
  sensor_measurement_available = false;
  last_seen_cm4_write_seq = 0U;
  __DSB();
  SCB_CleanDCache_by_Addr((uint32_t *)sensor_shared_data, (int32_t)sizeof(*sensor_shared_data));
}
