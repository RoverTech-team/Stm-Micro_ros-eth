# Joint States Pub/Sub Implementation Summary

## Overview
This implementation adds joint states publishing and joint command subscribing capabilities to the micro-ROS firmware running on the CM7 core. The system publishes joint states (positions, velocities, efforts) as a Float32MultiArray on the "joint_states" topic and subscribes to joint commands on the "joint_commands" topic.

## Changes Made

### Makefile/CM7_hil/Makefile
1. Added JOINT_PUBLISH_RATE_HZ definition:
   ```
   JOINT_PUBLISH_RATE_HZ ?= 500
   ```
   This defines the default joint states publish rate at 500 Hz.

2. Added JOINT_PUBLISH_RATE_HZ to compiler flags:
   ```
   -DJOINT_PUBLISH_RATE_HZ=$(JOINT_PUBLISH_RATE_HZ)
   ```
   This passes the publish rate as a preprocessor definition to the CM7 firmware.

3. Fixed gateway IP configuration:
   - Verified that MICROROS_GATEWAY_IP_A-D definitions are correctly set
   - Ensured proper IP address formatting in networking setup

### main.c Changes

#### New Constants
```c
#define JOINT_COUNT 6u
#define JOINT_PUBLISH_PERIOD_MS 20U
```

#### New Global Variables
- Joint states publisher and message structures:
  ```c
  static rcl_publisher_t joint_states_publisher;
  static std_msgs__msg__Float32MultiArray joint_states_msg;
  static float joint_states_data[64U];
  static char joint_states_buffer[256U];
  ```
- Joint command subscription and executor:
  ```c
  static rcl_subscription_t joint_commands_subscription;
  static rclc_executor_t joint_command_executor;
  static std_msgs__msg__Float32MultiArray joint_commands_msg;
  static char joint_commands_buffer[256U];
  ```
- Joint command data arrays:
  ```c
  static float joint_commanded_positions[JOINT_COUNT];
  static float joint_commanded_velocities[JOINT_COUNT];
  static float joint_commanded_efforts[JOINT_COUNT];
  static uint32_t joint_command_seq;
  static volatile bool joint_command_received;
  ```
- Task handles:
  ```c
  static osThreadId_t jointStatesTaskHandle;
  static osThreadId_t jointCommandExecutorTaskHandle;
  ```

#### New Functions
1. `ParseJointCommand` - Parses incoming joint command messages
2. `BuildJointStatesJson` - Builds JSON representation and Float32MultiArray data for joint states
3. `JointCommandCallback` - Callback for joint command subscription
4. `StartJointStatesTask` - Task that publishes joint states at configured rate
5. `StartJointCommandExecutorTask` - Task that processes joint command subscriptions

#### SetupNetworkingAndMicroRos Updates
- Added joint states publisher initialization:
  ```c
  joint_states_publisher = rcl_get_zero_initialized_publisher();
  joint_states_pub_ret = rclc_publisher_init_default(
      &joint_states_publisher,
      &heartbeat_node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
      "joint_states");
  ```
- Added joint command subscription initialization:
  ```c
  joint_commands_subscription = rcl_get_zero_initialized_subscription();
  joint_commands_sub_ret = rclc_subscription_init_default(
      &joint_commands_subscription,
      &heartbeat_node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
      "joint_commands");
  ```
- Added joint command executor initialization:
  ```c
  joint_command_executor = rclc_executor_get_zero_initialized_executor();
  joint_command_executor_ret = rclc_executor_init(&joint_command_executor, &heartbeat_support.context, 1, &allocator);
  joint_command_executor_ret = rclc_executor_add_subscription(
      &joint_command_executor,
      &joint_commands_subscription,
      &joint_commands_msg,
      &JointCommandCallback,
      ON_NEW_DATA);
  ```
- Initialized joint state data structures:
  ```c
  joint_states_msg.data.data = joint_states_data;
  joint_states_msg.data.size = 0U;
  joint_states_msg.data.capacity = sizeof(joint_states_data) / sizeof(float);
  memset(joint_states_buffer, 0, sizeof(joint_states_buffer));
  memset(joint_commanded_positions, 0, sizeof(joint_commanded_positions));
  memset(joint_commanded_velocities, 0, sizeof(joint_commanded_velocities));
  memset(joint_commanded_efforts, 0, sizeof(joint_commanded_efforts));
  joint_command_seq = 0U;
  joint_command_received = false;
  ```

#### MX_FREERTOS_Init Updates
- Added joint states task creation:
  ```c
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
  ```
- Added joint command executor task creation:
  ```c
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
  ```

## Resource Usage
- **Memory**: 
  - Joint states data buffer: 64 floats (256 bytes)
  - Joint states JSON buffer: 256 bytes
  - Joint commands buffer: 256 bytes
  - Task stacks: 2 × 4096 bytes (8192 bytes total)
- **CPU**: 
  - Joint states task runs at 500 Hz (every 2ms)
  - Joint command executor task runs every 20ms
- **Additional**: 
  - One additional publisher and subscription
  - One additional executor

## Build Verification
To verify the build:
1. Clean build directory: `make clean`
2. Build firmware: `make` (or specific target like `make -f Makefile/CM7_hil/Makefile`)
3. Check for successful compilation without warnings/errors
4. Verify binary size increase is reasonable (<50KB additional)

## Testing Commands
1. Build and flash firmware:
   ```bash
   make -f Makefile/CM7_hil/Makefile
   ```

2. Verify joint states topic is published:
   ```bash
   ros2 topic echo /joint_states
   ```

3. Send joint commands to test subscription:
   ```bash
   ros2 topic pub /joint_commands std_msgs/msg/Float32MultiArray "{data: [1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}"
   ```

4. Monitor debug output via USB serial:
   ```bash
   screen /dev/tty.usbmodem* 115200
   ```

5. Check for runtime faults via LED status or debug output