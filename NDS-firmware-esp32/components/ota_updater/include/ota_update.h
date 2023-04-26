#ifndef _OTA_UPDATE_H_
#define _OTA_UPDATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <esp_err.h>

void initialize_ota_setup(void);
esp_err_t Activate_OTA(void);

#endif