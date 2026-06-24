/* micro-ROS Heartbeat Test over Ethernet
 * Setup stays in main(), runtime behavior is owned by FreeRTOS tasks.
 */

#include "main.h"
#include "cmsis_os.h"
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
#include <rmw/types.h>
#include <uxr/client/transport.h>
#include <rcutils/allocator.h>
#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>

#include "microros_transports.h"
#include "microros_sim_network.h"

extern struct netif gnetif;

#define HEARTBEAT_PERIOD_MS 500U
#define TELEMETRY_BUFFER_SIZE 160U
#define TIME_SYNC_BUFFER_SIZE 192U

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

static osThreadId_t statusLedTaskHandle;
static osThreadId_t setupTaskHandle;
static osThreadId_t heartbeatPublisherTaskHandle;

static rclc_support_t heartbeat_support;
static rcl_node_t heartbeat_node;
static rcl_publisher_t heartbeat_publisher;
static rcl_publisher_t heartbeat_telemetry_publisher;
static rcl_publisher_t time_sync_echo_publisher;
static rcl_subscription_t time_sync_request_subscription;
static std_msgs__msg__Int32 heartbeat_msg;
static std_msgs__msg__String heartbeat_telemetry_msg;
static std_msgs__msg__String time_sync_request_msg;
static std_msgs__msg__String time_sync_echo_msg;
static char heartbeat_telemetry_buffer[TELEMETRY_BUFFER_SIZE];
static char time_sync_request_buffer[TIME_SYNC_BUFFER_SIZE];
static char time_sync_echo_buffer[TIME_SYNC_BUFFER_SIZE];

static bool SetupNetworkingAndMicroRos(void);
static void TimeSyncRequestCallback(const void *msg_in);
static void InitHighResolutionClock(void);
static uint64_t GetMonotonicTimeUs(void);
static size_t AppendLiteral(char *buffer, size_t capacity, size_t offset, const char *text);
static size_t AppendUnsignedLong(char *buffer, size_t capacity, size_t offset, unsigned long value);
static size_t AppendSignedLong(char *buffer, size_t capacity, size_t offset, long value);
static size_t AppendUint64(char *buffer, size_t capacity, size_t offset, uint64_t value);
static bool ParseJsonUnsignedLongField(const char *json, const char *field_name, unsigned long *value);
static bool ParseJsonUint64Field(const char *json, const char *field_name, uint64_t *value);
static bool PublishTelemetrySample(
  rcl_publisher_t *publisher,
  std_msgs__msg__String *message,
  char *buffer,
  size_t buffer_size,
  const char *topic_name,
  uint32_t sequence,
  int32_t value);
static void ProcessPendingTimeSyncRequests(void);

static void InitHighResolutionClock(void)
{
  /* Renode does not emulate DWT cycle counting reliably for this target.
   * Keep the timing source simple and valid under emulation. */
}

static uint64_t GetMonotonicTimeUs(void)
{
  if(osKernelGetState() == osKernelRunning)
  {
    return ((uint64_t)osKernelGetTickCount()) * 1000ULL;
  }

  return ((uint64_t)HAL_GetTick()) * 1000ULL;
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
  int failure_count = 0;
  const uint64_t heartbeat_period_us = ((uint64_t)HEARTBEAT_PERIOD_MS) * 1000ULL;
  uint64_t next_release_us = GetMonotonicTimeUs();
  (void)argument;

  printf("CM7: publisher-task-start\r\n");

  for(;;)
  {
    uint64_t now_us;
    uint64_t remaining_us;

    ProcessPendingTimeSyncRequests();
    now_us = GetMonotonicTimeUs();
    if(now_us < next_release_us)
    {
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
      continue;
    }

    rcl_ret_t ret = rcl_publish(&heartbeat_publisher, &heartbeat_msg, NULL);

    if(ret == RCL_RET_OK)
    {
      if(!PublishTelemetrySample(
           &heartbeat_telemetry_publisher,
           &heartbeat_telemetry_msg,
           heartbeat_telemetry_buffer,
           sizeof(heartbeat_telemetry_buffer),
           "heartbeat",
           (uint32_t)heartbeat_msg.data,
           heartbeat_msg.data) &&
         healthy_publish_seen)
      {
        SetRuntimeFault("heartbeat-telemetry-publish");
      }
      printf("CM7: publish-ok seq=%ld\r\n", (long)heartbeat_msg.data);
      heartbeat_msg.data++;
      failure_count = 0;

      if(!healthy_publish_seen)
      {
        healthy_publish_seen = true;
        SetFirmwareStatus(STATUS_RUNNING_SOLID_GREEN);
      }
    }
    else
    {
      failure_count++;
      printf("CM7: publish-failed ret=%ld count=%d\r\n", (long)ret, failure_count);

      if(healthy_publish_seen)
      {
        SetRuntimeFault("publish");
      }
      else if(failure_count >= 10)
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

    next_release_us += heartbeat_period_us;
    now_us = GetMonotonicTimeUs();
    while(next_release_us <= now_us)
    {
      next_release_us += heartbeat_period_us;
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

static void ProcessPendingTimeSyncRequests(void)
{
  if(!publisher_ready)
  {
    time_sync_request_msg.data.size = 0U;
    return;
  }

  while(true)
  {
    rmw_message_info_t message_info = rmw_get_zero_initialized_message_info();
    rcl_ret_t take_ret;

    time_sync_request_msg.data.size = 0U;
    if(time_sync_request_msg.data.capacity > 0U)
    {
      time_sync_request_msg.data.data[0] = '\0';
    }

    take_ret = rcl_take(
      &time_sync_request_subscription,
      &time_sync_request_msg,
      &message_info,
      NULL);

    if(take_ret == RCL_RET_OK)
    {
      size_t size = time_sync_request_msg.data.size;
      size_t capacity = time_sync_request_msg.data.capacity;
      if(time_sync_request_msg.data.data != NULL && capacity > 0U)
      {
        if(size >= capacity)
        {
          size = capacity - 1U;
          time_sync_request_msg.data.size = size;
        }
        time_sync_request_msg.data.data[size] = '\0';
      }

      TimeSyncRequestCallback(&time_sync_request_msg);
      continue;
    }

    break;
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
  rcl_ret_t heartbeat_telemetry_pub_ret;
  rcl_ret_t time_sync_echo_pub_ret;
  rcl_ret_t time_sync_request_sub_ret;
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

  time_sync_request_msg.data.data = time_sync_request_buffer;
  time_sync_request_msg.data.size = 0U;
  time_sync_request_msg.data.capacity = sizeof(time_sync_request_buffer);
  time_sync_request_buffer[0] = '\0';

  heartbeat_msg.data = 0;
  heartbeat_telemetry_msg.data.data = heartbeat_telemetry_buffer;
  heartbeat_telemetry_msg.data.size = 0U;
  heartbeat_telemetry_msg.data.capacity = sizeof(heartbeat_telemetry_buffer);
  heartbeat_telemetry_buffer[0] = '\0';
  time_sync_echo_msg.data.data = time_sync_echo_buffer;
  time_sync_echo_msg.data.size = 0U;
  time_sync_echo_msg.data.capacity = sizeof(time_sync_echo_buffer);
  time_sync_echo_buffer[0] = '\0';
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
  if(htim != NULL && htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
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
