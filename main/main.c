#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <uni.h>

#include "src/hardware/motor/motor.h"
#include "src/hardware/sensor/sensor_distancia.h"
#include "src/robot/robot_task.h"
#include "src/bluetooth/bt_control.h"
#include "src/utils/config.h"

void app_main(void) {
    // 1. Inicializa Memória (Essencial para o Bluetooth salvar o pareamento)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Inicializa Hardware do Robô
    motor_init_stby(PIN_STBY);
    motor_init(get_motor_direito());
    motor_init(get_motor_esquerdo());
    inicializar_sensores();
    robot_init();

    // 3. Cria Task do Robô (roda em paralelo com Bluetooth)
    xTaskCreate(robot_logic_task, "robot_task", 4096, NULL, 5, NULL);

    // 4. Inicializa Bluetooth
    btstack_init();
    uni_platform_set_custom(get_my_platform());
    uni_init(0, NULL);

    printf("--- Bluetooth Iniciado. Aguardando Xbox... ---\n");

    // 5. Entra no loop do BTstack
    btstack_run_loop_execute();
}
