
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


// Prototypes of the tasks and functions
void sensortask(void *args);
void print_task(void *args);
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
char morse_message[1024] = "";
int morse_index = 0;
bool STATE_end_message = false;

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    
    init_button1();
    init_button2();

    init_led();
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_fxn);
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_RISE, true, button_fxn);

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
        printf("__Task creation failed\n__");
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
        printf("__Failed to initialize ICM-42670P.\n__");
        vTaskDelete(NULL);
        return;
    }

    if (ICM42670_start_with_default_values() != 0){
        printf("__ICM-42670P could not initialize accelerometer or gyroscope\n__");
        vTaskDelete(NULL);
        return;
    }

    printf("__IMU initialized__\n");
    
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
            //printf("Failed to read imu data, please put machine to known state\n"); // miksi tämä tulee välillä?
            programState = STATE_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void button_fxn(uint gpio, uint32_t events){
    if (gpio == BUTTON1) {
        button_pressed = true;
        toggle_led();
    } else if (gpio == BUTTON2) {
        STATE_end_message = true;
        button_pressed = true;
    }
    
    // kikkailee ledin jos jaksaa/ehtii
}

void print_task(void *args){
    
    for(;;){
        if (programState == STATE_orientation_changed && button_pressed && !STATE_end_message) {
            if (currentOrientation == pysty) {
                //printf("__.__");
                morse_message[morse_index++] = '.';
            } else if (currentOrientation == vaaka) {
                //printf("__-__");
                morse_message[morse_index++] = '-';
            } else if (currentOrientation == kylki) {
                //printf("__ __");
                morse_message[morse_index++] = ' ';
            } else {
            printf("Orientation: Unknown\n");
            }   
            button_pressed = false;
            programState = STATE_RUNNING;
            toggle_led();
        } else if (STATE_end_message) {
            printf("  \n");
            morse_message[morse_index++] = ' ';
            morse_message[morse_index++] = ' '; 
            morse_message[morse_index++] = '\n';
            morse_message[morse_index] = '\0'; 
            for (int i = 0; i < morse_index; i++) {
                printf("%c", morse_message[i]);
            }
            morse_index = 0;
            morse_message[0] = '\0';
            button_pressed = false;
            STATE_end_message = false;
            programState = STATE_RUNNING;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}