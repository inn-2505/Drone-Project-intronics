#include <stdio.h>
#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
static const char *TAG = "TIME_DEBUG";


#define RED GPIO_NUM_21
#define YELLOW GPIO_NUM_22
#define GREEN GPIO_NUM_23

uint32_t time_10ms = 0;   
uint32_t time_100ms = 0;
uint32_t time_1s = 0;
uint32_t time_1min = 0;
uint64_t now = 0;
uint64_t last_time = 0;

bool GREENSTATE = false;
bool YELLOWSTATE = false;
bool REDSTATE = false;

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "System initialized. Starting Timer...");
    last_time = esp_timer_get_time();
    gpio_reset_pin(GREEN);
    gpio_reset_pin(RED);

    gpio_set_direction (YELLOW,GPIO_MODE_OUTPUT);
    gpio_set_direction(RED, GPIO_MODE_OUTPUT);
    gpio_set_direction (GREEN,GPIO_MODE_OUTPUT);


    while(1)
    {
        now = esp_timer_get_time();
        
        if (now - last_time >= 10000)
        { 
            last_time += 10000; 
            time_10ms++; 

            // ------------ do something every 10ms ----------------

            if (time_10ms >= 10) 
            {
                time_10ms = 0; 
                time_100ms++;

                // ------------ do something every 100ms ----------------
                //YELLOWSTATE = !YELLOWSTATE;
                //gpio_set_level(YELLOW, YELLOWSTATE);
                REDSTATE = !REDSTATE;     
                gpio_set_level(RED, REDSTATE);
                
                ESP_LOGI(TAG,  "[100 MS] -> time_100ms = %lu, RED LED = %s", time_100ms, REDSTATE ? "ON" : "OFF");
                if (time_100ms >= 10) 
                {   
                    time_100ms = 0;
                    time_1s++;
                    
                    // ------------ do something every 1s ----------------
                    GREENSTATE = !GREENSTATE;
                    gpio_set_level(GREEN, GREENSTATE);
                    
                    
                    
                    ESP_LOGI(TAG, "[1 SEC] -> time_1s = %lu, GREEN LED = %s", time_1s, GREENSTATE ? "ON" : "OFF");

                    if (time_1s >= 60)
                    {
                        time_1s = 0;
                        time_1min++;

                        // ------------ do something every 1min ----------------
                        
                    } 
                }      
            }    
        }
        
    }
}
