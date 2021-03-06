/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "user_config.h"
#include "ota.h"
#if tcp_server
	#include "tcp.h"
#else
	#include "udp_server.h"
#endif


/**********************************************************************/

#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID

#define DEFAULT_LAN_PORT 	12476

#define SMART_LED_PIN_NUM         14
#define SMART_LED_PIN_FUNC        FUNC_GPIO14
#define SMART_LED_PIN_MUX         PERIPHS_IO_MUX_MTMS_U

#define SMART_KEY_PIN_NUM         12
#define SMART_KEY_PIN_FUNC        FUNC_GPIO12
#define SMART_KEY_PIN_MUX         PERIPHS_IO_MUX_MTDI_U

#define RELAY_PIN_NUM         	  4
#define RELAY_PIN_FUNC        	  FUNC_GPIO4
#define RELAY_PIN_MUX         	  PERIPHS_IO_MUX_GPIO4_U

#define Smart_LED_ON  GPIO_OUTPUT_SET(GPIO_ID_PIN(SMART_LED_PIN_NUM), 0);
#define Smart_LED_OFF GPIO_OUTPUT_SET(GPIO_ID_PIN(SMART_LED_PIN_NUM), 1);

#define RELAY_ON  GPIO_OUTPUT_SET(GPIO_ID_PIN(RELAY_PIN_NUM), 1);
#define RELAY_OFF GPIO_OUTPUT_SET(GPIO_ID_PIN(RELAY_PIN_NUM), 0);
LOCAL os_timer_t flash_light_timer;


uint8 mqtt_buff[200];				//mqtt接收数据缓存
uint8 pub_topic[50],sub_topic[50];	//mqtt发布和订阅主题
uint8 service_topic[50];			//向服务器返回状态主题
uint8 pub_flag=0;
uint8 on_off_flag=0;
uint8 dev_sta=0;					//设备状态
extern uint8 long_pass_flag;				//长按标志

uint8 send_serv;

extern uint8 tcp_send;

uint8 local_ip[15];					//记录本地IP，用于station模式的tcp service

uint8 dev_sid[15];					//记录设备SID


LOCAL os_timer_t serv_timer;
/*************************************
 *倒计时相关变量
 */

LOCAL os_timer_t onoff_timer;
uint16 sec=0,min=0;
uint8 down_flag=0;
/*************************************
 *定时相关变量
 */

typedef struct
{
	uint16 tm_year;
	uint8 tm_mon;
	uint8 tm_day;
	uint8 tm_hour;
	uint8 tm_min;
	uint8 tm_sec;
	uint8 tm_wday;
} tm;
tm now_timedate;

LOCAL os_timer_t socket_timer;


uint8 wifi_socket_timing[22][50];		//存储定时器数据

uint8 now_time[10];						//记录当前时间

uint8 timing_day[22][8];				//记录重复天数
uint8 timing_ontime[22][6];			//记录定时开启时间
uint8 timing_offtime[22][6];			//记录定时关闭时间
uint8 timing_timersta[22][5];			//记录定时器状态
uint8 list[1200];						//存储定时器列表
uint8 timer=0;							//记录定时器序号


struct	softap_config	ap_config;
LOCAL os_timer_t sys_restart_timer;

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

//按键相关
static struct keys_param switch_param;
static struct single_key_param *switch_signle;

char temp_str[30];    // 临时子串，查找字符串相关
#if smartconfig
	uint8_t  lan_buf[200];
	uint16_t lan_buf_len;
	uint8 	 udp_sent_cnt = 0;
#endif
LOCAL os_timer_t pub_timer;;
LOCAL os_timer_t check_ip_timer;

const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};



void ICACHE_FLASH_ATTR  socket_timer_callback();
/****************************************************************************
						MQTT
******************************************************************************/
MQTT_Client mqttClient;
typedef unsigned long u32_t;
static ETSTimer sntp_timer;

void  ICACHE_FLASH_ATTR sys_restart_timer_callback()
{
	static uint8 i;
	i++;
	if(i>6)
	{
		i=0;
		os_timer_disarm(&sys_restart_timer);
		system_restart();
	}
}
void sys_restart()
{
	 os_timer_disarm(&sys_restart_timer);
	 os_timer_setfn(&sys_restart_timer, (os_timer_func_t *)sys_restart_timer_callback, NULL);
	 os_timer_arm(&sys_restart_timer, 1000, 1);//1s
}


void sntpfn()
{
    u32_t ts = 0;
    static uint8 timer_start=0;
    char* current_time="Wed Dec 07 16:34:45 2016";
    char chsec[3]={""};
    char chmin[3]={""};
    char chhour[3]={""};
    char chwday[4]={""};

    ts = sntp_get_current_timestamp();
    current_time=sntp_get_real_time(ts);

    if (ts == 0)
    {
        //os_printf("did not get a valid time from sntp server\n");
    } else
    {

/***************************获取到网络时间 后开启定时任务**********************************/
    	if(timer_start==0)
    	{
    		timer_start=1;
			os_timer_disarm(&socket_timer);
			os_timer_setfn(&socket_timer, (os_timer_func_t *)socket_timer_callback, NULL);
			os_timer_arm(&socket_timer, 1000, 1);//1s
    	}
/***********************************************************************************/
    	os_strncpy(now_time,current_time+11,5);//保存  时：分
    	//os_printf("current time : %s\n", now_time);

    	os_strncpy(chsec,current_time+17,2);//秒
		os_strncpy(chmin,current_time+14,2);//分
		os_strncpy(chhour,current_time+11,2);//时
		os_strncpy(chwday,current_time,3);//周
#if 0
		os_strncpy(chmday,current_time+8,2);//天
		os_strncpy(chmon,current_time+4,3);//月
		os_strncpy(chyear,current_time+20,4);//年

		os_printf("current time : %s\n", current_time);

		//月份转换
		if(strcmp(chmon,"Jan")==0)
		{
			now_timedate.tm_mon=1;
		}
		else if(strcmp(chmon,"Feb")==0)
		{
			now_timedate.tm_mon=2;
		}
		else if(strcmp(chmon,"Mar")==0)
		{
			now_timedate.tm_mon=3;
		}
		else if(strcmp(chmon,"Apr")==0)
		{
			now_timedate.tm_mon=4;
		}
		else if(strcmp(chmon,"May")==0)
		{
			now_timedate.tm_mon=5;
		}
		else if(strcmp(chmon,"Jun")==0)
		{
			now_timedate.tm_mon=6;
		}
		else if(strcmp(chmon,"Jul")==0)
		{
			now_timedate.tm_mon=7;
		}
		else if(strcmp(chmon,"Aug")==0)
		{
			now_timedate.tm_mon=8;
		}
		else if(strcmp(chmon,"Sep")==0)
		{
			now_timedate.tm_mon=9;
		}
		else if(strcmp(chmon,"Oct")==0)
		{
			now_timedate.tm_mon=10;
		}
		else if(strcmp(chmon,"Nov")==0)
		{
			now_timedate.tm_mon=11;
		}
		else if(strcmp(chmon,"Dec")==0)
		{
			now_timedate.tm_mon=12;
		}
#endif

		now_timedate.tm_hour=(chhour[0]-'0')*10+(chhour[1]-'0');
		now_timedate.tm_min=(chmin[0]-'0')*10+(chmin[1]-'0');
		now_timedate.tm_sec=(chsec[0]-'0')*10+(chsec[1]-'0');
		//周转换
		if(strcmp(chwday,"Mon")==0)
			now_timedate.tm_wday=1+'0';
		else if(strcmp(chwday,"Tue")==0)
			now_timedate.tm_wday=2+'0';
		else if(strcmp(chwday,"Wed")==0)
			now_timedate.tm_wday=3+'0';
		else if(strcmp(chwday,"Thu")==0)
			now_timedate.tm_wday=4+'0';
		else if(strcmp(chwday,"Fri")==0)
			now_timedate.tm_wday=5+'0';
		else if(strcmp(chwday,"Sat")==0)
			now_timedate.tm_wday=6+'0';
		else if(strcmp(chwday,"Sun")==0)
			now_timedate.tm_wday=7+'0';
    }

}
void ICACHE_FLASH_ATTR
my_sntp_init(void)
{
	 sntp_setservername(0, "pool.ntp.org");        // set sntp server after got ip address
	 sntp_init();
	 os_timer_disarm(&sntp_timer);
	 os_timer_setfn(&sntp_timer, (os_timer_func_t *)sntpfn, NULL);
	 os_timer_arm(&sntp_timer, 1000, 1);//1s
}

void ICACHE_FLASH_ATTR  wifiConnectCb(uint8_t status)
{


	/*struct ip_info info; //用于获取IP地址的信息
    if(status == STATION_GOT_IP){
    	wifi_get_ip_info(STATON_IF,&info);
    	station_server_init(&info.ip,8888);

    } else {
          MQTT_Disconnect(&mqttClient);
    }*/
}



void mqttConnectedCb(uint32_t *args)
{
	uint8 init_buff[200];
	os_sprintf(init_buff,"{\"cmd\":\"i am ok\",\"dev\":\"socket\",\"sys_ver\":\"%s\",\"hard_ver\":\"%s\",\"sid\":\"%s\"}"
			,SYS_VER,HARD_VER,dev_sid);
    MQTT_Client* client = (MQTT_Client*)args;
#if 1
    INFO("MQTT: Connected\r\n");
#endif
    wifi_set_opmode_current(0x01);
    MQTT_Subscribe(client,  sub_topic, 1);
    MQTT_Publish(&mqttClient,  pub_topic,init_buff, os_strlen(init_buff), 0, 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;

#if 1
    INFO("MQTT: Disconnected\r\n");
#endif
}

void mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;

    pub_flag=0;
    //abc=1;
    os_memset(mqtt_buff,0,sizeof(mqtt_buff));
#if 1
    INFO("MQTT: Published\r\n");
#endif
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1),
            *dataBuf = (char*)os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    os_memset(mqtt_buff,0,sizeof(mqtt_buff));
    os_memcpy(dataBuf, data, data_len);
    os_memcpy(mqtt_buff, data, data_len);
    dataBuf[data_len] = 0;
    pub_flag=1;
#if 0
    INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
#endif
    os_free(topicBuf);
    os_free(dataBuf);
}


#if smartconfig
/******************************************************************************
 	 	 	 	 	 	 	 	 SMART_CONFIG
 ******************************************************************************/



LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;

	if ((udp_sent_cnt++) >5) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}

	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;

	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;

	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);

        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		os_memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;

		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);

		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");

		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}

		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            os_printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            os_printf("SC_STATUS_FIND_CHANNEL\n");
            Smart_LED_OFF;
            MQTT_Disconnect(&mqttClient);
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
			sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            os_printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;

	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            os_printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                os_memcpy(phone_ip, (uint8*)pdata, 4);
                os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
            }
            Smart_LED_ON;
            smartconfig_stop();
            sys_restart();
            break;
    }

}


#endif
/******************倒计时回调******************************/
void  ICACHE_FLASH_ATTR onoff_timer_callback()
{
	if(sec==0)
	{
		if(min==0)
		{
			if(down_flag==1)
			{
				dev_sta=1;
			}
			else
				dev_sta=0;
			on_off_flag=1;
			min=0;
			os_timer_disarm(&onoff_timer);
			return;
		}
		sec=60;
		min--;
	}
	sec--;
}

void ICACHE_FLASH_ATTR  save_flash(uint32 des_addr,uint32* data)
{
	spi_flash_erase_sector(des_addr);
	spi_flash_write(des_addr * SPI_FLASH_SEC_SIZE,data, sizeof(data));
}
void  ICACHE_FLASH_ATTR load_flash(uint32 des_addr,uint32* data)
{
	spi_flash_read(des_addr * SPI_FLASH_SEC_SIZE,data, sizeof(data));
}

/***********************定时器******************************/
void  ICACHE_FLASH_ATTR socket_timer_callback()
{
	uint8 i;
	static uint8 count=0;
	for(i=1;i<22;i++)
	{
#if time_debug
		os_printf("timing_timersta[%d]=%s\r\n",i,timing_timersta[i]);
#endif
		if(strstr(timing_timersta[i],"on")!=NULL)
		{
			count++;
			if(strchr(timing_day[i],now_timedate.tm_wday))
			{
				if(strstr(timing_ontime[i],now_time)!=NULL&&now_timedate.tm_sec==0)
				{
					on_off_flag=1;
					dev_sta=1;//时间到，打开
				}
				if(strstr(timing_offtime[i],now_time)!=NULL&&now_timedate.tm_sec==0)
				{
					on_off_flag=1;
					dev_sta=0;//时间到，关闭
				}
			}
		}
	}
	if(count==0)
	{
#if 1
		os_printf("no timer is open!!!\r\n");
#endif
		os_timer_disarm(&socket_timer);
	}
	else
	{
#if 0
		os_printf("timer num is:%d\r\n",count);
#endif
		count=0;
	}
}



/************************获取定时器个数********************************/
uint8 ICACHE_FLASH_ATTR  get_timer()
{
	uint8 i,count=0;
	for(i=1;i<22;i++)
	{
		if(strstr(wifi_socket_timing[i],"time")!=NULL)
		{
			count++;
		}
	}
//	os_printf("count num is:%d\r\n",count);
	return count;
}
/****************************拼定时器列表*************************************************/
void ICACHE_FLASH_ATTR  send_list()
{
	uint8 i,count=0;
	os_sprintf(list,"{\"cmd\":\"wifi_socket_read_timing_ack\",\"sid\":\"%s\",\"data\":[",dev_sid);
	for(i=0;i<22;i++)
	{
		if(strstr(wifi_socket_timing[i],"time")!=NULL)
		{
			if(count>0)
				os_strcat(list,",");
			count++;
			os_strcat(list,wifi_socket_timing[i]);
		}
	}
	os_strcat(list,"]}");
#if 0
	os_printf("%s\n", list);
#endif
}
void ICACHE_FLASH_ATTR  serv_timer_callback()//用于发送状态给服务器，服务器保存当前状态
{
	send_serv=1;
	os_timer_disarm(&serv_timer);
	os_printf("send to serv\r\n");
}
/****************************************收到数据开始处理*******************************************/
void ICACHE_FLASH_ATTR  pub_timer_callback()
{
	uint8 frist_pos=0;
	uint16 i;
	uint8 shi,fen,miao;
	static uint8 pub_buff[240];		//发布数据缓存
	static uint8 state[10];
	static uint8 ip[4]={192,168,1,3};
	os_memset(state,0,os_strlen(state));

	if(pub_flag==1)
	{
		pub_flag=0;
		os_memset(pub_buff,0,os_strlen(pub_buff));
		if(strstr(mqtt_buff,dev_sid)!=NULL)
		{
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_count_down\"")!=NULL)//倒计时
			{
				frist_pos=GetSubStrPos(mqtt_buff,"\"data\":");

				shi=(mqtt_buff[frist_pos+8]-'0')*10+(mqtt_buff[frist_pos+9]-'0');
				fen=(mqtt_buff[frist_pos+11]-'0')*10+(mqtt_buff[frist_pos+12]-'0');
				miao=(mqtt_buff[frist_pos+14]-'0')*10+(mqtt_buff[frist_pos+15]-'0');
				min=shi*60+fen;
				sec=miao;

				if(strstr(mqtt_buff,"\"state\":\"on\"")!=NULL)
				{
					down_flag=1;
					os_strcpy(state,"on");
				}
				if(strstr(mqtt_buff,"\"state\":\"off\"")!=NULL)
				{
					down_flag=0;
					os_strcpy(state,"off");
				}
				if(strstr(mqtt_buff,"\"state\":\"cancel\"")!=NULL)
				{
					os_strcpy(state,"cancel");
					sec=0;
					min=0;
					os_timer_disarm(&onoff_timer);
				}
				else
				{
					 os_timer_disarm(&onoff_timer);
					 os_timer_setfn(&onoff_timer, (os_timer_func_t *)onoff_timer_callback, NULL);
					 os_timer_arm(&onoff_timer, 1000, 1);//1000ms
				}
				os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_count_down_ack\",\"state\":\"%s\",\"data\":\"%02d,%02d,%02d\",\"sid\":\"%s\"}",state,shi,fen,miao,dev_sid);
				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);
			}
/****************************获取倒计时状态**********************************/
			if(strstr(mqtt_buff,"\"wifi_socket_read_down\"")!=NULL)
			{
				if(dev_sta==1)
				{
					os_strcpy(state,"off");
				}
				else
					os_strcpy(state,"on");
				os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_read_down_ack\",\"state\":\"%s\",\"data\":\"%02d,%02d,%02d\",\"sid\":\"%s\"}",state,min/60,min%60,sec,dev_sid);
				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);
			}
/*****************************设置定时****************************************************/
			//{"cmd":"wifi_socket_timing","day":"1234567","ontime":"10:00","offtime":"19:00","timer":1,"timer_state":"on","sid":"12345678"}
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_timing\"")!=NULL)//定时器
			{
				frist_pos=GetSubStrPos(mqtt_buff,"\"timer\":");
				if(mqtt_buff[frist_pos+9]>='0'&&mqtt_buff[frist_pos+9]<='9')
				{
					timer=(mqtt_buff[frist_pos+8]-'0')*10+(mqtt_buff[frist_pos+9]-'0');//有多少个定时
				}
				else
					timer=(mqtt_buff[frist_pos+8]-'0');//有多少个定时

				/******************************存定时列表******************************************/
				//获取定时数量，如果大于20个，清空当前的定时，上传超过20
				if(get_timer()>19&&timer==0)
				{
					os_memset(pub_buff,0,os_strlen(pub_buff));
					os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_timeout_ack\",\"sid\":\"%s\"}",dev_sid);
				}
				else
				{
					if(timer==0)//代表新建的定时器
					{
						for(i=1;i<24;i++)
						{
							if(strstr(wifi_socket_timing[i],"time")==NULL)
							{
								timer=i;//保存新建定时的位置
								break;
							}
						}
					}
					frist_pos=GetSubStrPos(mqtt_buff,"\"day\":");
					os_strncpy(timing_day[timer],mqtt_buff+frist_pos+7,7);//保存第N个定时的重复的天数

					if(strstr(mqtt_buff,"\"timer_state\":\"on\"")!=NULL)
					{
						os_strcpy(state,"\"on\"");
						os_strcpy(timing_timersta[timer],"on");
					}
					else
					{
						os_strcpy(state,"\"off\"");
						os_strcpy(timing_timersta[timer],"off");
					}
					if(strstr(mqtt_buff,"\"ontime\":\"close\"")!=NULL)
					{
						os_strcpy(timing_ontime[timer],"close");//
					}
					else
					{
						frist_pos=GetSubStrPos(mqtt_buff,"\"ontime\":");
						os_strncpy(timing_ontime[timer],mqtt_buff+frist_pos+10,5);

					}
					if(strstr(mqtt_buff,"\"offtime\":\"close\"")!=NULL)
					{
						os_strcpy(timing_offtime[timer],"close");
					}
					else
					{
						frist_pos=GetSubStrPos(mqtt_buff,"\"offtime\":");
						os_strncpy(timing_offtime[timer],mqtt_buff+frist_pos+11,5);
					}
					/*************************************开启定时任务*******************************************/
					os_timer_disarm(&socket_timer);
					os_timer_setfn(&socket_timer, (os_timer_func_t *)socket_timer_callback, NULL);
					os_timer_arm(&socket_timer, 1000, 1);//1s

					os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_timing_ack\",\"day\":\"%s\",\"ontime\":\"%s\",\"offtime\":\"%s\",\"timer\":%d,\"timer_state\":%s,\"sid\":\"%s\"}",
							timing_day[timer],timing_ontime[timer],timing_offtime[timer],timer,state,dev_sid);

					os_memset(wifi_socket_timing[timer],0,os_strlen(wifi_socket_timing[timer]));

					os_sprintf(wifi_socket_timing[timer],"{\"time\":\"%s,%s,%s,%s\",\"timer\":%d}",timing_ontime[timer],
								timing_offtime[timer],timing_day[timer],timing_timersta[timer],timer);
				}
				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);

			}
/************************************读定时列表*****************************************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_read_timing\"")!=NULL)
			{
				send_list();

				spi_flash_erase_sector(CFG_LOCATION + 5);
				spi_flash_write((CFG_LOCATION + 5) * SPI_FLASH_SEC_SIZE,(uint32 *)list,sizeof( list));

				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(list,os_strlen(list));
#else
					WIFI_UDP_SendNews(list,os_strlen(list));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,list, os_strlen(list), 0, 0);

			}
/*****************************************删除定时*****************************************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_del_timing\"")!=NULL)
			{
				frist_pos=GetSubStrPos(mqtt_buff,"\"timer\":");
				if(mqtt_buff[frist_pos+9]>='0'&&mqtt_buff[frist_pos+9]<='9')
				{
					timer=(mqtt_buff[frist_pos+8]-'0')*10+(mqtt_buff[frist_pos+9]-'0');//
				}
				else
					timer=(mqtt_buff[frist_pos+8]-'0');//获取要删除的定时
				/****************************全清空该定时器的状态******************************************/
				os_memset(wifi_socket_timing[timer],0,os_strlen(wifi_socket_timing[timer]));
				os_memset(timing_day[timer],0,os_strlen(timing_day[timer]));
				os_memset(timing_ontime[timer],0,os_strlen(timing_ontime[timer]));
				os_memset(timing_offtime[timer],0,os_strlen(timing_offtime[timer]));
				os_memset(timing_timersta[timer],0,os_strlen(timing_timersta[timer]));

				os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_del_timing_ack\",\"timer\":%d,\"sid\":\"%s\"}",timer,dev_sid);
				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);

			}
/************************************读开关状态************************************************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_read\"")!=NULL)
			{
				on_off_flag=1;
			}
/*****************************************ota**********************************************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_update\"")!=NULL)
			{
				os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_update_ack\",\"sid\":\"%s\"}",dev_sid);
				if(tcp_send==1)
				{
					tcp_send=0;
#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
				}
				else
					MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);
				ota_start_Upgrade(ip, 80,"8266update/WiFi_Socket/");
			}
/**********************获取IP*************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_equipment_ping\"")!=NULL)
			{
				os_sprintf(pub_buff,"{\"cmd\":\"wifi_equipment_ping_ack\",\"ip\":\"%s\",\"sid\":\"%s\"}",local_ip,dev_sid);
				MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);
			}
			/****************清空定时列表*************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_clear\"")!=NULL)
			{
				if(spi_flash_erase_sector(CFG_LOCATION + 5)==SPI_FLASH_RESULT_OK)
				{
					system_restart();
				}
			}
			/**********************开关**************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket\"")!=NULL)
			{
				if(strstr(mqtt_buff,"\"on\"")!=NULL)
				{
					dev_sta=1;
				}
				if(strstr(mqtt_buff,"\"off\"")!=NULL)
				{
					dev_sta=0;
				}
				on_off_flag=1;
			}
		}

	}
	if(on_off_flag==1)
	{
		if(dev_sta==0)
		{
			RELAY_OFF;
			os_strcpy(state,"\"off\"");
		}
		else
		{
			os_strcpy(state,"\"on\"");
			RELAY_ON;
		}
		os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_ack\",\"state\":%s,\"sys_ver\":\"%s\",\"hard_ver\":\"%s\",\"sid\":\"%s\"}",
				state,SYS_VER,HARD_VER,dev_sid);
		if(tcp_send==1)
		{
			tcp_send=0;

#if tcp_server
					WIFI_TCP_SendNews(pub_buff,os_strlen(pub_buff));
#else
					WIFI_UDP_SendNews(pub_buff,os_strlen(pub_buff));
#endif
			//***********向服务器，3S后发送数据给服务器保存数据，连续触发UDP开关后，会不断刷新清零定时器*************************/
			os_timer_disarm(&serv_timer);
			os_timer_setfn(&serv_timer, (os_timer_func_t *)serv_timer_callback, NULL);
			os_timer_arm(&serv_timer, 3000, 1);//3000ms
			//****************************************************************/
		}
		else
			MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);

		save_flash(CFG_LOCATION + 4,(uint32 *)&dev_sta);
		on_off_flag=0;
	}
	if(send_serv==1)
	{
		MQTT_Publish(&mqttClient,  service_topic,pub_buff, os_strlen(pub_buff), 0, 0);
		send_serv=0;
	}

}
/********************************配网指示灯闪烁回调函数**********************************************/
void ICACHE_FLASH_ATTR  flash_light_timer_callback()
{
	static uint8 flag=0;
	if(flag==0)
	{
		flag=1;
		Smart_LED_OFF;
	}
	else
	{
		flag=0;
		Smart_LED_ON;
	}
}


void ICACHE_FLASH_ATTR dhcps_lease(void)
{

	struct	dhcps_lease	dhcp_lease;
	struct ip_info info;
	wifi_softap_dhcps_stop();//设置前关闭DHCP
	IP4_ADDR(&dhcp_lease.start_ip,192,168,5,1);

	IP4_ADDR(&dhcp_lease.end_ip,192,168,5,100);

	IP4_ADDR(&info.ip, 192, 168, 5, 1);
	IP4_ADDR(&info.gw, 192, 168, 5, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &info);

	wifi_softap_set_dhcps_lease(&dhcp_lease);
	wifi_softap_dhcps_start();

}

/*
 * 函数名:void Wifi_AP_Init()
 * 功能wifi_ap初始化
 */
void ICACHE_FLASH_ATTR WIFIAPInit()
{
    struct softap_config apConfig;

    /***************************模式设置************************************/
         if(wifi_set_opmode(0x03)){          //  设置为AP模式

         }else{

         }
    /***************************名字设通道置************************************/
	  os_bzero(&apConfig, sizeof(struct softap_config));
	  wifi_softap_get_config(&apConfig);
	  apConfig.ssid_len=0;                      //设置ssid长度
	  os_memset(apConfig.ssid,' ',strlen(apConfig.ssid));

	  os_sprintf(apConfig.ssid,"grasp_socket-%s",dev_sid);			//设置ssid名字

	 // os_strcpy(apConfig.password,"12345678");  //设置密码
	 // apConfig.authmode =3;                     //设置加密模式
	  wifi_softap_set_config(&apConfig);        //配置

	  dhcps_lease();
}
//长按按键开始配网
static void Switch_LongPress_Handler( void )
{
#if tcp_server
#else
		struct ip_info ipConfig;
		uint8 ip[10];
#endif

#if smartconfig
		wifi_station_dhcpc_start();
		smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS); //SC_TYPE_ESPTOUCH,SC_TYPE_AIRKISS,SC_TYPE_ESPTOUCH_AIRKISS
		wifi_set_opmode(STATION_MODE);
		smartconfig_start(smartconfig_done);
#endif
		Smart_LED_OFF;
		long_pass_flag=1;
		os_timer_disarm(&sntp_timer);
		os_timer_disarm(&check_ip_timer);
/****************************配网指示灯开始闪烁*****************************************/
		os_timer_disarm(&flash_light_timer);
		os_timer_setfn(&flash_light_timer, (os_timer_func_t *)flash_light_timer_callback, NULL);
		os_timer_arm(&flash_light_timer, 300, 1);//300ms
/*******************************************************************************/
		wifi_station_disconnect();
		MQTT_Disconnect(&mqttClient);
		os_delay_us(60000);
		WIFIAPInit();
#if tcp_server
		WIFIServerMode();
#else
		 udp_server_init(8888,&ipConfig.ip,0);
#endif
}

//短按开启/断开开关
static void Switch_ShortPress_Handler( void )
{
	if(long_pass_flag==2)
	{
		long_pass_flag=0;
		system_restart();
	}
	else if(long_pass_flag==1)
	{
		long_pass_flag=2;
	}
	else
	{
		pub_flag=1;
		on_off_flag=1;
		if(dev_sta==0)
		{
			dev_sta=1;
		}
		else
			dev_sta=0;
	}
}

void ICACHE_FLASH_ATTR  gpio_init(void)
{
	 PIN_FUNC_SELECT(SMART_LED_PIN_MUX,SMART_LED_PIN_FUNC);//LED
	 PIN_FUNC_SELECT(RELAY_PIN_MUX,RELAY_PIN_FUNC);//RELAY
	 //按键配置
	switch_signle = key_init_single( SMART_KEY_PIN_NUM, SMART_KEY_PIN_MUX,
									 SMART_KEY_PIN_FUNC,
									  &Switch_LongPress_Handler ,
									 &Switch_ShortPress_Handler
									  );
	 switch_param.key_num = 1;
	 switch_param.single_key = &switch_signle;
	 key_init( &switch_param );

	 Smart_LED_ON;
}


/************************************************************************


获取字符串中某个子字符串的首字母的下标

*************************************************************************/
//
void ICACHE_FLASH_ATTR  ReadStrUnit(char * str,char *temp_str,int idx,int len)
{
   static uint16 index = 0;
   for(index=0; index < len; index++)
   {
       temp_str[index] = str[idx+index];
   }
   temp_str[index] = '\0';
}

int  ICACHE_FLASH_ATTR GetSubStrPos(char *str1,char *str2)
{
   int idx = 0;
   int len1 = strlen(str1);
   int len2 = strlen(str2);

   if( len1 < len2)
   {
       os_printf("error 1 \n");
       return -1;
   }

   while(1)
   {
       ReadStrUnit(str1,temp_str,idx,len2);
       if(strcmp(str2,temp_str)==0)break;
       idx++;
       if(idx>=len1)return -1;
   }

   return idx;
}



/***********************************************
 初始化
 **********************************/
void  ICACHE_FLASH_ATTR MQTT_Init()
{
	os_sprintf(sub_topic,"iotbroad/iot/socket/%s",dev_sid);
	os_sprintf(pub_topic,"iotbroad/iot/socket_ack/%s",dev_sid);
	os_sprintf(service_topic,"iotbroad/iot/dev/socket_ack/%s",dev_sid);
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);

	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}

/*************************************脸上wifi后检测IP*******************************************/
void  ICACHE_FLASH_ATTR check_ip_timer_callback()
{
	static uint8_t wifiStatus = STATION_IDLE,flag=0;
	struct ip_info ipConfig;
	uint8 ip[10];

	wifi_get_ip_info(STATION_IF, &ipConfig);
	wifiStatus = wifi_station_get_connect_status();
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{
		if(flag==1)
		{
			flag=0;
			wifi_set_opmode(0x01);
			 my_sntp_init();//获取网络时间
#if tcp_server
			 station_server_init(&ipConfig.ip,8888);
#else
			 udp_server_init(8888,&ipConfig.ip,0);
#endif
			 os_memcpy(ip,&ipConfig.ip,4);

			 os_sprintf(local_ip,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);

			 MQTT_Connect(&mqttClient);
		}
		 //os_timer_disarm(&check_ip_timer);
	}
	else
		flag=1;
}

void  ICACHE_FLASH_ATTR check_ip()
{
	 os_timer_disarm(&check_ip_timer);
	 os_timer_setfn(&check_ip_timer, (os_timer_func_t *)check_ip_timer_callback, NULL);
	 os_timer_arm(&check_ip_timer, 100, 1);//100ms
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        	 rf_cal_sec = 512 - 5;
        	 break;

        case FLASH_SIZE_16M_MAP_1024_1024:
			rf_cal_sec = 512 - 5;
			break;

		case FLASH_SIZE_32M_MAP_512_512:
			rf_cal_sec = 1024 - 5;
			break;
		case FLASH_SIZE_32M_MAP_1024_1024:
			rf_cal_sec = 1024 - 5;
			break;

		case FLASH_SIZE_64M_MAP_1024_1024:
			rf_cal_sec = 2048 - 5;
			break;
		case FLASH_SIZE_128M_MAP_1024_1024:
			rf_cal_sec = 4096 - 5;
			break;
		default:
			rf_cal_sec = 0;
			break;
    }

    return rf_cal_sec;
}



void  ICACHE_FLASH_ATTR to_scan(void)
{
	uint8 i,frist_pos=0,len=0;
	uint8 data=0;
	uint8 *buff=os_malloc(1000);
	uint8 *cache=os_malloc(1000);
	/***************************导入flash数据并解析****************************************/
	if(strstr(list,"timer")!=NULL)
	{
		frist_pos=GetSubStrPos(list,"[");
		os_strncpy(buff,list+frist_pos+1,os_strlen(list)-frist_pos-1);
		for(i=0;i<22;i++)
		{
			frist_pos=GetSubStrPos(buff,"timer");
			if(buff[frist_pos+8]>='0'&&buff[frist_pos+8]<='9')
				data=(buff[frist_pos+7]-'0')*10+(buff[frist_pos+8]-'0');
			else
				data=buff[frist_pos+7]-'0';
			frist_pos=GetSubStrPos(buff,"}");
			os_strncpy(wifi_socket_timing[data],buff,frist_pos+1);//截取一包数据{"time":"10:00,16:00,0034560,on","timer":2}

			os_strncpy(timing_ontime[data],wifi_socket_timing[data]+9,5);
			os_strncpy(timing_offtime[data],wifi_socket_timing[data]+15,5);
			os_strncpy(timing_day[data],wifi_socket_timing[data]+21,7);
			os_strncpy(timing_timersta[data],wifi_socket_timing[data]+29,3);

			os_memset(cache,0,sizeof(cache));
			os_strncpy(cache,buff+frist_pos+2,os_strlen(buff)-frist_pos-2);//将截取的数据存在cache中
			os_memset(buff,0,sizeof(buff));//清空buff
			os_strcpy(buff,cache);//将截取到的数据重新拷贝到buff
			//os_printf("buff=%s\r\n",buff);

#if time_debug
			os_printf("wifi_socket_timing[%d]=%s\r\n",data,wifi_socket_timing[data]);
			os_printf("timing_ontime[%d]=%s\r\n",data,timing_ontime[data]);
			os_printf("timing_offtime[%d]=%s\r\n",data,timing_offtime[data]);
			os_printf("timing_day[%d]=%s\r\n",data,timing_day[data]);
			os_printf("timing_timersta[%d]=%s\r\n",data,timing_timersta[data]);
#endif
			len=os_strlen(buff);
			if(len<5)
				break;
		}
	}
	os_free(buff);
	os_free(cache);
	/***************************开启任务**********************************/
	os_timer_disarm(&pub_timer);
	os_timer_setfn(&pub_timer, (os_timer_func_t *)pub_timer_callback, NULL);
	os_timer_arm(&pub_timer, 200, 1);//200ms

}
/* Create a bunch of objects as demonstration. */

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	os_delay_us(60000);

   	os_sprintf(dev_sid,"%x%x",system_get_chip_id(),spi_flash_get_id());

   	WIFIAPInit();

	CFG_Load();

	MQTT_Init();

	gpio_init();

	//spi_flash_erase_sector(CFG_LOCATION + 5);
	load_flash(CFG_LOCATION + 4,(uint32 *)&dev_sta);

	on_off_flag=1;
	spi_flash_read((CFG_LOCATION + 5) * SPI_FLASH_SEC_SIZE,(uint32 *)list, sizeof(list));
#if 0
	os_printf("list=%s\r\n",list);
#endif
 //检测到连接ip之后连接mqtt
	check_ip();

	wifi_set_sleep_type(MODEM_SLEEP_T);

   	system_init_done_cb(to_scan);
	INFO("\r\nSystem started ...\r\n");
	os_printf("SYS_Ver is %s,HARD_Ver is %s\r\n",SYS_VER,HARD_VER);
	os_printf("devsid is %s\r\n",dev_sid);
}



