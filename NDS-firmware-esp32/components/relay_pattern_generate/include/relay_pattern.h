#ifndef _relay_pattern_h_
#define _relay_pattern_h_

#include <stdio.h>

void Serial_Timer_Callback(void *params);
void Random_Timer_Callback(void *params);
void Activate_Relays();
void setup_relay_update_task();

#endif