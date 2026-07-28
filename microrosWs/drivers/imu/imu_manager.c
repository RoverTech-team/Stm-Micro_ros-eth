/**
 * @file imu_manager.c
 * @brief High-level IMU management implementation for STM32H7 micro-ROS application
 */

#include "imu_manager.h"
#include "lsm6dsv16x_stm32_hal.h"
#include <string.h>
#include <math.h>

bool IMU_Manager_InitI2C(imu_manager_t *mgr, void *hi2c_handle, uint8_t i2c_7bit_addr)
{
    if (mgr == NULL || hi2c_handle == NULL) {
        return false;
    }

    memset(mgr, 0, sizeof(imu_manager_t));

    int32_t ret = lsm6dsv16x_stm32_init_i2c(&mgr->sensor_ctx, (I2C_HandleTypeDef *)hi2c_handle, i2c_7bit_addr);
    if (ret != 0) {
        return false;
    }

    /* Check WHO_AM_I register (expects 0x70) */
    if (!lsm6dsv16x_check_whoami(&mgr->sensor_ctx)) {
        return false;
    }

    /* Reset sensor to clean state */
    lsm6dsv16x_reset(&mgr->sensor_ctx);

    /* Configure 120Hz ODR, ±4g Accel, ±500dps Gyro */
    ret = lsm6dsv16x_configure(&mgr->sensor_ctx,
                                LSM6DSV16X_XL_ODR_120Hz, LSM6DSV16X_4g,
                                LSM6DSV16X_G_ODR_120Hz, LSM6DSV16X_500dps);
    if (ret != 0) {
        return false;
    }

    mgr->is_initialized = true;
    return true;
}

bool IMU_Manager_Update(imu_manager_t *mgr)
{
    if (mgr == NULL || !mgr->is_initialized) {
        return false;
    }

    int32_t ret = lsm6dsv16x_read_scaled(&mgr->sensor_ctx, &mgr->latest_data);
    if (ret != 0) {
        return false;
    }

    mgr->sample_sequence++;

    /* Update shared memory struct if available */
    volatile shared_data_t *sdata = SHARED_DATA;
    if (sdata != NULL && sdata->magic == SHARED_MAGIC) {
        sdata->imu_accel_x_mg  = (int32_t)lroundf((mgr->latest_data.accel_x / STANDARD_GRAVITY) * 1000.0f);
        sdata->imu_accel_y_mg  = (int32_t)lroundf((mgr->latest_data.accel_y / STANDARD_GRAVITY) * 1000.0f);
        sdata->imu_accel_z_mg  = (int32_t)lroundf((mgr->latest_data.accel_z / STANDARD_GRAVITY) * 1000.0f);

        sdata->imu_gyro_x_mdps = (int32_t)lroundf((mgr->latest_data.gyro_x / DEG_TO_RAD) * 1000.0f);
        sdata->imu_gyro_y_mdps = (int32_t)lroundf((mgr->latest_data.gyro_y / DEG_TO_RAD) * 1000.0f);
        sdata->imu_gyro_z_mdps = (int32_t)lroundf((mgr->latest_data.gyro_z / DEG_TO_RAD) * 1000.0f);

        sdata->imu_temp_mdegc  = (int32_t)lroundf(mgr->latest_data.temp_c * 1000.0f);

        sdata->imu_seq        = mgr->sample_sequence;
        sdata->imu_data_ready = 1U;
    }

    return true;
}

void IMU_Manager_BuildMsg(const imu_manager_t *mgr, float *output_array, size_t array_capacity)
{
    if (mgr == NULL || output_array == NULL || array_capacity < IMU_FLOAT_COUNT) {
        return;
    }

    output_array[0] = mgr->latest_data.accel_x;
    output_array[1] = mgr->latest_data.accel_y;
    output_array[2] = mgr->latest_data.accel_z;

    output_array[3] = mgr->latest_data.gyro_x;
    output_array[4] = mgr->latest_data.gyro_y;
    output_array[5] = mgr->latest_data.gyro_z;

    output_array[6] = mgr->latest_data.temp_c;
}

#ifndef STANDALONE
bool IMU_Manager_InitPublisher(rcl_publisher_t *publisher,
                               rcl_node_t *node,
                               std_msgs__msg__Float32MultiArray *msg,
                               float *data_buffer,
                               size_t buffer_size)
{
    if (publisher == NULL || node == NULL || msg == NULL || data_buffer == NULL) {
        return false;
    }

    *publisher = rcl_get_zero_initialized_publisher();
    rcl_ret_t pub_ret = rclc_publisher_init_default(
        publisher,
        node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
        "imu/data_raw");

    if (pub_ret != RCL_RET_OK) {
        return false;
    }

    msg->data.data     = data_buffer;
    msg->data.size     = IMU_FLOAT_COUNT;
    msg->data.capacity = buffer_size;

    return true;
}
#endif
