
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"


#define DEFAULT_TASK_STACK_SIZE  2048

enum state { 
    STATE_IDLE,
    STATE_RUNNING,
    STATE_ERROR
};

enum state programState = STATE_IDLE;


float pitch = 0.0f;
float roll = 0.0f;
float yaw = 0.0f;
uint32_t lastTime = 0;

int main() {
    stdio_init_all();
    init_i2c_default();

    if (init_ICM42670() != 0) {
        printf("Failed to initialize ICM-42670P.\n");
        return -1;
    }

    if (ICM42670_start_with_default_values() != 0){
        printf("ICM-42670P could not initialize accelerometer or gyroscope\n");
        return -1;
    }

    printf("Imu yhistetty");

    uint32_t last_time = to_ms_since_boot(get_absolute_time());
    while (1) {
        float ax, ay, az, gx, gy, gz, t;
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            uint32_t current_time = to_ms_since_boot(get_absolute_time());
            float dt = (current_time - last_time) / 1000.0f; // Convert ms to seconds
            last_time = current_time;

            // Integrate gyroscope data to get angles
            pitch += gx * dt;
            roll  += gy * dt;
            yaw   += gz * dt;

            printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f | Temp: %2.2f°C | Pitch: %f, Roll: %f, Yaw: %f\n", 
                   ax, ay, az, gx, gy, gz, t, pitch, roll, yaw);
        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
//toimi nyt vittu