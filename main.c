#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_PIN 14  // Definir el GPIO 14 como salida

void app_main(void) {
    // Reiniciar y configurar GPIO 14 como salida
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_PIN, 1); // Encender LED
        vTaskDelay(pdMS_TO_TICKS(1000)); // Esperar 1 segundo

        gpio_set_level(LED_PIN, 0); // Apagar LED
        vTaskDelay(pdMS_TO_TICKS(1000)); // Esperar 2 segundos
    }
}
