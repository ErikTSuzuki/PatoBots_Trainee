idf_component_register(
    SRCS "main.c"
    	"../src/hardware/motor/motor.c"
        "../src/hardware/sensor/sensor_distancia.c"
        "../src/robot/robot_task.c"
        "../src/bluetooth/my_platform.c"
        "../src/tsop/ir_control.c"
    INCLUDE_DIRS "."
    	"../src"
       	"../src/utils"
       	"../src/hardware/motor"
       	"../src/hardware/sensor"
       	"../src/robot"
       	"../src/bluetooth"
       	"../src/tsop"
       	"../components/bluepad32/external/btstack/port/esp32/components/btstack"
		"../components/bluepad32/external/btstack/port/esp32/components/btstack/include"
    REQUIRES esp_adc esp_driver_ledc esp_driver_gpio bluepad32 driver
)
