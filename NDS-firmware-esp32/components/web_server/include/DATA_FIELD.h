#ifndef _DATA_FIELD_H_
#define _DATA_FIELD_H_
#include "stdio.h"
#include "stdbool.h"

/*************** structure of System_login approval *********************************/
typedef struct resp_tt
{
    bool approve;
} resp_t;

resp_t response; // Use carefully  // only global access for login authentication

/*************** structure of Setting_password *********************************/
typedef struct settings_username_tt
{
    char current_username[32];
    char new_username[32];
    char confirm_username[32];
} settings_username_t;

/*************** structure of Setting_username *********************************/
typedef struct settings_pass_name_tt
{
    char current_password[32];
    char new_password[32];
    char confirm_password[32];
    char new_username[32];
} settings_pass_name_t;

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

/*************** structure of Relay Status *********************************/
typedef enum Relay_Status
{
    RELAY_UPDATE_1 = 1,
    RELAY_UPDATE_2,
    RELAY_UPDATE_3,
    RELAY_UPDATE_4,
    RELAY_UPDATE_5,
    RELAY_UPDATE_6,
    RELAY_UPDATE_7,
    RELAY_UPDATE_8,
    RELAY_UPDATE_9,
    RELAY_UPDATE_10,
    RELAY_UPDATE_11,
    RELAY_UPDATE_12,
    RELAY_UPDATE_13,
    RELAY_UPDATE_14,
    RELAY_UPDATE_15,
    RELAY_UPDATE_16,
    RANDOM_UPDATE,
    SERIAL_UPDATE,
    RELAY_UPDATE_MAX
} e_Relay_Status_t;

/* Default values of LoginCreds*/
#define default_username "adminuser"
#define default_password "adminpass"

/* System LED ON - OFF */
#define SYS_LED 2
#define SYS_LED_OFF 0
#define SYS_LED_ON 1
/* Relay ON - OFF */
#define NUM_OF_RELAY 16
#define NUM_OF_LED_RELAY 12
#define R_ON 0
#define R_OFF 1

#endif