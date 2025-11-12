
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"


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
    pysty,
    vaaka,
    kylki
};

enum state programState = STATE_IDLE;
enum orientation currentOrientation = pysty;
bool lahetys = false;


int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    programState = STATE_RUNNING;
    TaskHandle_t sensorTask = NULL;
    TaskHandle_t printTask = NULL;
    TaskHandle_t buttonTask = NULL;
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

    init_button1();
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_fxn);
        /*
    BaseType_t result3 = xTaskCreate(button_task,       // (en) Task
                "button",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &buttonTask);    // (en) A handle to control the execution of this task*/

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
            if (az > 0.8 || az < -0.8) {
                currentOrientation = vaaka;
                // Vois asettaa myös x:lle tarkistuksen
                // toimis myös kylellään
                programState = STATE_orientation_changed;
            } else if (ay > 0.8 || ay < -0.8) {
                currentOrientation = pysty;
                programState = STATE_orientation_changed;
            } else if (ax > 0.8 || ax < -0.8) {
                currentOrientation = kylki;
                programState = STATE_orientation_changed;
                //lahetys = true;
            }

        } else {
            printf("Failed to read imu data\n");
            
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void button_task(void *args){
    for(;;){
        lahetys = gpio_get(SW1_PIN);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
   //lahetys = true;
}

void button_fxn(uint gpio, uint32_t events){
    lahetys = true;
}

void print_task(void *args){
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(500));
        if (programState == STATE_orientation_changed && lahetys) {
            if (currentOrientation == pysty) {
            printf(".");
            } else if (currentOrientation == vaaka) {
                printf("-");
            } else if (currentOrientation == kylki) {
                printf(" ");
            } else {
            printf("Orientation: Unknown\n");
            }   
            lahetys = false;
            programState = STATE_RUNNING;
        }
    }
    
}