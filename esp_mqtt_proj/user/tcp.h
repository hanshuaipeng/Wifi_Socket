#ifndef _tcp_H
#define _tcp_H

void WIFIAPInit();
void WIFIServerMode();
void TcpServerListen_PCon(void *arg);
void WIFI_TCP_SendNews(unsigned char *dat,uint16 len);
void dhcps_lease(void);
void WIFI_TCP_SendNews(unsigned char *dat,uint16 len);
void ICACHE_FLASH_ATTR station_server_init(struct ip_addr *local_ip,int port);
#endif

