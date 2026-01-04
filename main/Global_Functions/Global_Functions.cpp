#include "Global_Functions.h"

void Init_GPIO_Isr()
{
    static bool gpio_isr_installed = false;
    if(!gpio_isr_installed) {
        gpio_install_isr_service(0);
        gpio_isr_installed = true;
    }
}