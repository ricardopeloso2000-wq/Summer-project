#ifndef GLOBAL_FUNC_H
#define GLOBAL_FUNC_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

void Init_GPIO_Isr();

#endif