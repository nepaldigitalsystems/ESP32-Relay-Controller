#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "driver/gpio.h"

#define led 2

void init_led(void)
{
    gpio_pad_select_gpio(led);
    gpio_set_direction(led,GPIO_MODE_OUTPUT);
}
void toggle_login_led(bool isOn)
{
    gpio_set_level(led, isOn);
}