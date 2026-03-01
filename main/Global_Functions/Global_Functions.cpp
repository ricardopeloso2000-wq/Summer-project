#include "Global_Functions.h"

void Init_GPIO_Isr()
{
    static SemaphoreHandle_t lock = nullptr;
    static bool installed = false;

    if(lock == nullptr) {
        lock = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(lock, portMAX_DELAY);

    if(!installed) {
        gpio_install_isr_service(0);
        installed = true;
    }

    xSemaphoreGive(lock);
}