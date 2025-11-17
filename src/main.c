/*
* Introduction to Computer Systems 2025
* Morse code transmitter using FreeRTOS on Raspberry Pi Pico
* Authors: Oskari Koivumäki, Jonne Kemppainen, Joakim Brännlund
*/
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
enum orientation currentOrientation = VERTICAL; // current orientation of the machine
char morse_message[MESSAGE_BUFFER_SIZE] = "";  // buffer for morse message
int message_index = 0;                  // current index in the message buffer


/*
* Initializes all componenents and creates FreeRTOS tasks and interuprions
* Tasks: sensortask, print_task
* Interrupts: button_fxn for button 1 and button 2
*/
int main() {

    // Initialize stdio
    stdio_init_all();
    
    // Initialize HAT SDK, initializes i2c for imu sensor
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    
    // Initialize buttons and led
    init_button1();
    init_button2();

    init_led();
    
    // Set up button interrupts
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_fxn);
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_RISE, true, button_fxn);

    // Create the tasks with xTaskCreate
    TaskHandle_t sensorTask = NULL;
    TaskHandle_t printTask = NULL;
    
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

/*
* Task to read data from imu sensor
* Uses ICM42670 sensor to read machine orientation
* Sets flag orientation_ready when new orientation is available from sensor data
*/
void sensortask(void *args){
    (void) args;
    
    if (init_ICM42670() != 0) {
        printf("__Failed to initialize IMU sensor\n__");
    }

    if (ICM42670_start_with_default_values() != 0){
        printf("__Failed to initialize IMU sensor\n__");
    }

    // Infinite loop to read sensor data
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

// Handler for button interrupts
void button_fxn(uint gpio, uint32_t events){
    if (gpio == BUTTON1) {
        button1_pressed = true;
    } else if (gpio == BUTTON2) {
        button2_pressed = true;
    }
}

/*
* Task to process and print morse messages
* Waits for button presses and orientation data to build message
* Button1 adds morse symbol to message buffer based on machine orientation and increments buffer index
* Button2 sends the message to client and resets the message buffer and index
* Blinks led one time for added symbol, two times for sent message
*/
void print_task(void *args){
    (void) args;

    for(;;){
        if (orientation_ready && button1_pressed) {
            // check for buffer overflow.
            // Final message needs space for two spaces, newline and null character
            if (message_index >= MESSAGE_BUFFER_SIZE - 4) {
                printf("__Message buffer full. Please press button 2 to send message__\n");
                button1_pressed = false;
                orientation_ready = false;
                continue;
            }

            // Add morse symbol to buffer based on orientation
            if (currentOrientation == VERTICAL) {
                morse_message[message_index++] = '.';
            } else if (currentOrientation == HORIZONTAL) {
                morse_message[message_index++] = '-';
            } else if (currentOrientation == SIDE) {
                morse_message[message_index++] = ' ';
            }

            // return to wait for next input
            button1_pressed = false;
            orientation_ready = false;

            // Inform user of added symbol
            blink_led(1);
        } else if (button2_pressed) {
            // Add the final symbols to match the message protocol
            morse_message[message_index++] = ' ';
            morse_message[message_index++] = ' '; 
            morse_message[message_index++] = '\n';
            morse_message[message_index] = '\0'; 

            // Print the morse message by iterating through the buffer
            for (int i = 0; i < message_index; i++) {
                printf("%c", morse_message[i]);
            }

            // Reset for next message
            message_index = 0;
            morse_message[0] = '\0';
            button1_pressed = false;
            button2_pressed = false;

            // Inform user of sent message
            blink_led(2);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}