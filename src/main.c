
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include "tkjhat/sdk.h"

/*
** Pittää muistaa kommentoija funktiot
*/
#define DEFAULT_STACK_SIZE  2048

// Prototypes of the tasks
void sensortask(void *args);
void print_task(void *args);
void button_task(void *args);
void button_fxn(uint gpio, uint32_t events);


enum state { 
    STATE_IDLE,
    STATE_RUNNING,
    STATE_orientation_changed,
    STATE_ERROR
};

enum orientation {
    // nämä vois kääntää lontooksi
    pysty,
    vaaka,
    kylki
};

enum state programState = STATE_IDLE;
enum orientation currentOrientation = pysty;
bool button_pressed = false;


int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    
    init_button1();
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_fxn);

    programState = STATE_RUNNING;
    TaskHandle_t sensorTask = NULL;
    TaskHandle_t printTask = NULL;
    
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensortask,       // (en) Task function
                "imu",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &sensorTask);    // (en) A handle to control the execution of this task

    BaseType_t result2 = xTaskCreate(print_task,       // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &printTask);    // (en) A handle to control the execution of this task

    if(result != pdPASS || result2 != pdPASS) {
        printf("Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

void sensortask(void *args){
    
    init_i2c_default();

    if (init_ICM42670() != 0) {
        printf("Failed to initialize ICM-42670P.\n");
        vTaskDelete(NULL);
        return;
    }

    if (ICM42670_start_with_default_values() != 0){
        printf("ICM-42670P could not initialize accelerometer or gyroscope\n");
        vTaskDelete(NULL);
        return;
    }

    printf("IMU initialized\n");
    
    for (;;) {
        float ax, ay, az, gx, gy, gz, temp;
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {

            //printf("Accel: X=%f, Y=%f, Z=%f \n", ax, ay, az);
            // vois olla vaikka abs ja arvot vois pistää vakioiksi
            if (az > 0.8 || az < -0.8) {
                currentOrientation = vaaka;
                programState = STATE_orientation_changed;
            } else if (ay > 0.8 || ay < -0.8) {
                currentOrientation = pysty;
                programState = STATE_orientation_changed;
            } else if (ax > 0.8 || ax < -0.8) {
                currentOrientation = kylki;
                programState = STATE_orientation_changed;
            }

        } else {
            printf("Failed to read imu data\n"); // miksi tämä tulee välillä?
            // State error?
            // Delete task?
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void button_fxn(uint gpio, uint32_t events){
    button_pressed = true;
    // kikkailee ledin jos jaksaa/ehtii
}

void print_task(void *args){
    /*
    ** TODO:
    ** SERIAL COMMUNICATION!
    */
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(50));
        if (programState == STATE_orientation_changed && button_pressed) {
            if (currentOrientation == pysty) {
            printf(".");
            } else if (currentOrientation == vaaka) {
                printf("-");
            } else if (currentOrientation == kylki) {
                printf(" ");
            } else {
            printf("Orientation: Unknown\n");
            }   
            button_pressed = false;
            programState = STATE_RUNNING;
        }
    }
}