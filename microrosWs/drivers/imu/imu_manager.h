/**
 * @file imu_manager.h
 * @brief High-level IMU management module for STM32H7 micro-ROS application
 */

#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lsm6dsv16x.h"
#include "shared_data.h"

#ifndef STANDALONE
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <std_msgs/msg/float32_multi_array.h>
#endif

#define IMU_FLOAT_COUNT 7U /* Accel X,Y,Z (m/s^2), Gyro X,Y,Z (rad/s), Temp (deg C) */

typedef struct {
    lsm6dsv16x_ctx_t sensor_ctx;
    lsm6dsv16x_data_scaled_t latest_data;
    uint32_t sample_sequence;
    bool is_initialized;
} imu_manager_t;

/**
 * @brief Initialize IMU manager over I2C hardware bus
 */
bool IMU_Manager_InitI2C(imu_manager_t *mgr, void *hi2c_handle, uint8_t i2c_7bit_addr);

/**
 * @brief Read IMU sensor data, update latest_data struct and shared_data memory
 */
bool IMU_Manager_Update(imu_manager_t *mgr);

/**
 * @brief Populate micro-ROS Float32MultiArray message with latest IMU data
 * Layout: [0..2] = accel (m/s^2), [3..5] = gyro (rad/s), [6] = temp (deg C)
 */
void IMU_Manager_BuildMsg(const imu_manager_t *mgr, float *output_array, size_t array_capacity);

#ifndef STANDALONE
/**
 * @brief Initialize micro-ROS publisher for IMU topic "/imu/data_raw"
 */
bool IMU_Manager_InitPublisher(rcl_publisher_t *publisher,
                               rcl_node_t *node,
                               std_msgs__msg__Float32MultiArray *msg,
                               float *data_buffer,
                               size_t buffer_size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* IMU_MANAGER_H */
