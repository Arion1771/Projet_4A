#ifndef MAIN_H
#define MAIN_H

#include "stm32wlxx_hal.h"

#define LED_GPIO_Port GPIOB
#define LED_Pin GPIO_PIN_5

void Error_Handler(void);

#endif /* MAIN_H */
