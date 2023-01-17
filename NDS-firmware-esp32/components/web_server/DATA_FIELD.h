#ifndef _DATA_FIELD_H_
#define _DATA_FIELD_H_
#include "stdbool.h"

/*************** global structure to store initial_user_wifi_connection (captive portal) *********************************/

typedef struct ap_config_tt
{
    char local_ssid[32];
    char local_pass[32];
} ap_config_t;

/*************** structure of payload *********************************/
typedef struct auth_tt
{
    char username[32];
    char password[32];
    char session[10];
} auth_t;

typedef struct resp_tt
{
    bool approve;
    bool write;
} resp_t;
resp_t response;

#endif