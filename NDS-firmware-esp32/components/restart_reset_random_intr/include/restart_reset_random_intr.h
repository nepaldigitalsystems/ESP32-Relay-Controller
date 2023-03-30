#ifndef _restart_reset_random_intr_
#define _restart_reset_random_intr_

#include "freertos/FreeRTOS.h"

void IRAM_ATTR restart_reset_random_isr(void *args);
void INIT_RS_PIN();
void INIT_RAND_PIN();


#endif