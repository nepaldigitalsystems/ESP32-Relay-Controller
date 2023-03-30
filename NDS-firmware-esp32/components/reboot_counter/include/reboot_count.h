#ifndef _reboot_count_h_
#define _reboot_count_h_
#include <stdio.h>

void RESTART_WIFI(uint8_t);
void Boot_count();
bool get_AP_RESTART();
bool get_STA_RESTART();

#endif
