#ifndef SENSOR_DISTANCIA_H_
#define SENSOR_DISTANCIA_H_

#include "esp_adc/adc_oneshot.h"

void inicializar_sensores(void);
int ler_sensor_raw(adc_channel_t canal);
int raw_para_cm(int raw);

#endif /* SENSOR_DISTANCIA_H_ */
