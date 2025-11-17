
#include <stdio.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include "tkjhat/sdk.h"
#include <math.h>

// Define constants
#define DEFAULT_STACK_SIZE  2048
#define MESSAGE_BUFFER_SIZE 256
#define IMU_THRESHOLD 0.8f

// Prototypes of the tasks and functions
void sensortask(void *args);
void print_task(void *args);
void button_fxn(uint gpio, uint32_t events);

// Machine orientation variables
enum orientation {
    VERTICAL,
    HORIZONTAL,
    SIDE
};

// Global variables
bool orientation_ready = false;     // true when orientation is read from imu sensor
bool button1_pressed = false;   // flag for button 1 press
bool button2_pressed = false;   // flag for button 2 press
enum orientation currentOrientation = VERTICAL;
char morse_message[MESSAGE_BUFFER_SIZE] = "";
uint8_t morse_index = 0;


int main() {
    stdio_init_all();
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    
    init_button1();
    init_button2();

    init_led();
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_fxn);
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_RISE, true, button_fxn);

    TaskHandle_t sensorTask = NULL;
    TaskHandle_t printTask = NULL;
    
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensortask,       
                "imu",             
                DEFAULT_STACK_SIZE, 
                NULL,               
                2,                  
                &sensorTask);    

    BaseType_t result2 = xTaskCreate(print_task,       
                "print",              
                DEFAULT_STACK_SIZE, 
                NULL,               
                2,                  
                &printTask);   

    if(result != pdPASS || result2 != pdPASS) {
        printf("__Task creation failed\n__");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

void sensortask(void *args){
    (void) args;
    
    if (init_ICM42670() != 0) {
        printf("__Failed to initialize IMU sensor\n__");
    }

    if (ICM42670_start_with_default_values() != 0){
        printf("__Failed to initialize IMU sensor\n__");
    }

    for (;;) {
        float ax, ay, az, gx, gy, gz, temp;
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {

            if (fabs(az) > IMU_THRESHOLD) {
                currentOrientation = HORIZONTAL;
                orientation_ready = true;
            } else if (fabs(ay) > IMU_THRESHOLD) {
                currentOrientation = VERTICAL;
                orientation_ready = true;
            } else if (fabs(ax) > IMU_THRESHOLD) {
                currentOrientation = SIDE;
                orientation_ready = true;
            }

        } else {
            printf("__Failed to read imu data__\n");    
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void button_fxn(uint gpio, uint32_t events){
    if (gpio == BUTTON1) {
        button1_pressed = true;
    } else if (gpio == BUTTON2) {
        button2_pressed = true;
    }
}

void print_task(void *args){
    (void) args;

    for(;;){
        if (orientation_ready && button1_pressed && !button2_pressed) {
            if (currentOrientation == VERTICAL) {
                morse_message[morse_index++] = '.';
            } else if (currentOrientation == HORIZONTAL) {
                morse_message[morse_index++] = '-';
            } else if (currentOrientation == SIDE) {
                morse_message[morse_index++] = ' ';
            }
            button1_pressed = false;
            orientation_ready = false;
            blink_led(1);
        } else if (button2_pressed) {
            morse_message[morse_index++] = ' ';
            morse_message[morse_index++] = ' '; 
            morse_message[morse_index++] = '\n';
            morse_message[morse_index] = '\0'; 
            for (int i = 0; i < morse_index; i++) {
                printf("%c", morse_message[i]);
            }
            morse_index = 0;
            morse_message[0] = '\0';
            button1_pressed = false;
            button2_pressed = false;
            blink_led(2);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}