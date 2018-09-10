#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_OPTIMIZE_PRINTF

#include "ets_sys.h"
#include "driver/uart.h"

#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "sntp.h"

#include "os_type.h"

#include "ip_addr.h"
#include "espconn.h"
#include "smartconfig.h"
#include "airkiss.h"

#include "driver/key.h"

#define debug 0
#define tcp_debug 	1
#define smartconfig 0

void ICACHE_FLASH_ATTR smartconfig_done(sc_status status, void *pdata);

int GetSubStrPos(char *str1,char *str2);
void wifiConnectCb(uint8_t status);
void sys_restart();
#endif

