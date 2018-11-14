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


uint8 mqtt_buff[200];				//mqtt闁规亽鍎查弫褰掑极閻楀牆绁︾紓鍌涙尭閻★拷
uint8 pub_topic[50],sub_topic[50];	//mqtt闁告瑦鍨电粩鐑藉椽瀹�鍐惧悅闂傚啫鎳嶇�靛本锛愰敓锟�
uint8 service_topic[50];			//闁告碍鍨跺﹢鍥礉閳ヨ櫕鐝ら弶鈺傛煥濞叉牠鎮╅懜纰夋嫹娴ｅ嘲鐦滃Λ甯嫹
uint8 pub_flag=0;
uint8 on_off_flag=0;
uint8 dev_sta=0;					//閻犱焦鍎抽ˇ顒勬偐閼哥鎷烽敓锟�
extern uint8 long_pass_flag;				//闂傦拷閹稿骸鐦婚柡宥呮搐缁伙拷

uint8 send_serv;

extern uint8 tcp_send;

uint8 local_ip[15];					//閻犱焦婢樼紞宥夊嫉椤掞拷濠�纰擯闁挎稑鐬奸弫銈嗙瀹告ⅸation婵☆垪锟藉磭纭�闁汇劌澧朿p service

uint8 dev_sid[15];					//閻犱焦婢樼紞宥囨媼閹屾УSID


LOCAL os_timer_t serv_timer;
/*************************************
 *闁稿﹥甯熼鎼佸籍閸撲焦绁查柛蹇撳暱瑜板鏌岄敓锟�
 */

LOCAL os_timer_t onoff_timer;
uint16 sec=0,min=0;
uint8 down_flag=0;
/*************************************
 *閻庤纰嶅鍌炴儎缁嬪灝褰犻柛娆愶耿閸ｏ拷
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


uint8 wifi_socket_timing[22][50];		//閻庢稒锚閸嬪秶锟借纰嶅鍌炲闯閵婏附娈堕柟鐧告嫹

uint8 now_time[10];						//閻犱焦婢樼紞宥堛亹閹惧啿顤呴柡鍐ㄧ埣濡拷

uint8 timing_day[22][8];				//閻犱焦婢樼紞宥夋煂瀹ュ拋妲诲鍨涙櫆閺嗭拷
uint8 timing_ontime[22][6];			//閻犱焦婢樼紞宥囷拷瑙勭濡炲倸顕ｉ敓浠嬪触椤栨稒顦ч梻鍌︽嫹
uint8 timing_offtime[22][6];			//閻犱焦婢樼紞宥囷拷瑙勭濡炲倿宕楅幎鑺ワ紨闁哄啫鐖煎Λ锟�
uint8 timing_timersta[22][5];			//閻犱焦婢樼紞宥囷拷瑙勭濡炲倿宕抽妸褍笑闁诡剨鎷�
uint8 list[1400];						//閻庢稒锚閸嬪秶锟借纰嶅鍌炲闯閵娿儱鐏欓悶娑虫嫹
uint8 timer=0;							//閻犱焦婢樼紞宥囷拷瑙勭濡炲倿宕抽妸銉х闁告瑱鎷�


struct	softap_config	ap_config;
LOCAL os_timer_t sys_restart_timer;

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

//闁圭顦甸弫顓㈡儎缁嬪灝褰�
static struct keys_param switch_param;
static struct single_key_param *switch_signle;

char temp_str[30];    // 濞戞挸鐡ㄥ鍌滐拷娑欏姃鐟曞棝鏁嶇仦鍓у弨闁瑰灚鍎抽悺褏绮敂鑳洬闁烩晝顭堥崣锟�

LOCAL os_timer_t pub_timer;;
LOCAL os_timer_t check_ip_timer;


void ICACHE_FLASH_ATTR  socket_timer_callback();
/****************************************************************************
						MQTT
******************************************************************************/
MQTT_Client mqttClient;
typedef unsigned long u32_t;
static ETSTimer sntp_timer;
//CRC16闁哄稄绻濋悰娆撳礄閼恒儲娈�
//CRC闁哄稄绻濋悰娆愭媴瀹ュ嫮绉靛ù锝呯Т濠�顏堝礈瀹ュ繒绀夊Δ鍌浢肩紞鍛村捶閵娿儲鍊�
uint16 ICACHE_FLASH_ATTR Ar_crc_16(uint8 *input,uint16 len)
{
	uint16 n = 0;
	uint8 m=0;
	uint16 crc_in = 0;
	uint16 crc_re = 0xffff;
	uint16 poly = 0xa001;
	uint16 xor_out = 0x0000;

    crc_in = input[len - 2] * 256 + input[len - 1];
    //os_printf("crc_in=0x%x\r\n",crc_in);// 婵炴潙顑堥惁顖涚閿濆洨鍨抽柨娑樻湰婢э箓宕￠惂鐪梒_in
	for(n=0;n<(len-2);n++)
	{
		crc_re = crc_re ^ input[n];
		for(m=0;m<8;m++)
		{
			if(crc_re & 1)
			{
				crc_re >>= 1;
				crc_re = crc_re ^ poly;
			}
			else
			{
				crc_re >>= 1;
			}
		}
	}
	crc_re = crc_re ^ xor_out;
	//os_printf("crc_re=0x%x\r\n",crc_re);// 婵炴潙顑堥惁顖涚閿濆洨鍨抽柨娑樻湰婢э箓宕￠惂鐪梒闁哄稄绻濋悰娆戯拷娑欘殙婵★拷
	if((crc_in == crc_re)||(crc_in == 0)){
		return crc_re;
	}
	else{
		return 0;
	}
}
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

/***************************闁兼儳鍢茶ぐ鍥礆閹殿喚绉圭紓浣圭矋濡炲倿姊婚敓锟� 闁告艾楠哥槐鎴﹀触椤栨氨鏆伴柡鍐╂构閹广垽宕濋敓锟�**********************************/
    	if(timer_start==0)
    	{
    		timer_start=1;
			os_timer_disarm(&socket_timer);
			os_timer_setfn(&socket_timer, (os_timer_func_t *)socket_timer_callback, NULL);
			os_timer_arm(&socket_timer, 1000, 1);//1s
    	}
/***********************************************************************************/
    	os_strncpy(now_time,current_time+11,5);//濞ｅ洦绻傞悺锟�  闁哄啳顔愮槐浼村礆閿燂拷
    	//os_printf("current time : %s\n", now_time);

    	os_strncpy(chsec,current_time+17,2);//缂佸鎷�
		os_strncpy(chmin,current_time+14,2);//闁告帪鎷�
		os_strncpy(chhour,current_time+11,2);//闁哄喛鎷�
		os_strncpy(chwday,current_time,3);//闁告冻鎷�
#if 0
		os_strncpy(chmday,current_time+8,2);//濠㈣鎷�
		os_strncpy(chmon,current_time+4,3);//闁哄牞鎷�
		os_strncpy(chyear,current_time+20,4);//妤犵儑鎷�

		os_printf("current time : %s\n", current_time);

		//闁哄牆鐗呴崬銈嗘姜椤掍礁搴�
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
		//闁告稏鍔忓ù鍡涘箲閿燂拷
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


	/*struct ip_info info; //闁烩偓鍔嬬花顒勬嚔瀹勬澘绲縄P闁革附婢樺鍐儍閸曨亙绻嗛柟顓ㄦ嫹
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
    os_memset(mqtt_buff,0,sizeof(mqtt_buff));
#if 1
    INFO("MQTT: Published\r\n");
#endif
}
void ICACHE_FLASH_ATTR CharToByte(uint8* pChar,uint8* pByte)
{
	uint8 h,l;
	h=pChar[0];//濡ゅ倹眉缂嶏拷
	l=pChar[1];//濞达絽绨肩紞锟�
	if(l>='0' && l<='9')
		l=l-'0';
	else if(l>='a' && l<='f')
		l=l-'a'+0xa;
	else if(l>='A' && l<='F')
		l=l-'A'+0xa;
	if(h>='0'&&h<='9')
		h=h-'0';
	else if(h>='a' && h<='f')
		h=h-'a'+0xa;
	else if(h>='A' &&h <='F')
		h=h-'A'+0xa;
	*pByte=h*16+l;
}
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1),
            *dataBuf = (char*)os_zalloc(data_len+1);
    uint16 crcout=0;
    uint16 len;
    uint8 a[2];
    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    if(data_len>300)
    {
    	for(len=0;len<data_len/2;len++)
    	{
    		os_strncpy(a,data+len*2,2);
    		CharToByte(a,dataBuf+len);
    		//os_printf("data[%d]=%x\r\n",len,dataBuf[len]);
    	}
    	if(dataBuf[0]==0xa5)
    	{
    		len=dataBuf[1]*256+dataBuf[2];
    		crcout=Ar_crc_16(dataBuf,len+6);
    		if(crcout)
			{
    			switch(dataBuf[3])
    			{
					case 1:
						spi_flash_erase_sector(0x77);
						if(spi_flash_write(0x77000,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
						{
							//system_restart();
						}
						break;
					case 2:
						spi_flash_erase_sector(0x78);
						if(spi_flash_write(0x78000,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
						{
							//system_restart();
						}
						break;
					case 3:
						if(spi_flash_write(0x78000+900,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
						{
							//system_restart();
						}
						break;
					default :break;
    			}
    			/*if(dataBuf[3]==1)
    			{
    				spi_flash_erase_sector(0x77);
					if(spi_flash_write(0x77000,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
					{
						//system_restart();
					}
    			}
    			else if(dataBuf[3]==2)
    			{
    				spi_flash_erase_sector(0x78);
					if(spi_flash_write(0x78000,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
					{
						//system_restart();
					}
    			}
    			else if(dataBuf[3]==3)
				{
					if(spi_flash_write(0x78000+900,(uint32*)dataBuf+1, len)==SPI_FLASH_RESULT_OK)
					{
						//system_restart();
					}
				}*/
			}
    	}
    }
    else
    {
    	os_memcpy(dataBuf, data, data_len);
    	os_memset(mqtt_buff,0,sizeof(mqtt_buff));
		os_memcpy(mqtt_buff, data, data_len);
		pub_flag=1;
    }
    dataBuf[data_len] = 0;
#if 0
    INFO("Receive topic: %s, data: %s \r\n", topicBuf, data);
#endif
    os_free(topicBuf);
    os_free(dataBuf);
}

/******************闁稿﹥甯熼鎼佸籍鐠虹儤绀�閻犲鎷�******************************/
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

/***********************閻庤纰嶅鍌炲闯閿燂拷******************************/
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
					dev_sta=1;//闁哄啫鐖煎Λ鍧楀礆鐢喚绀夐柟鍨尭缁憋拷
				}
				if(strstr(timing_offtime[i],now_time)!=NULL&&now_timedate.tm_sec==0)
				{
					on_off_flag=1;
					dev_sta=0;//闁哄啫鐖煎Λ鍧楀礆鐢喚绀夐柛蹇斿▕濡拷
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



/************************闁兼儳鍢茶ぐ鍥╋拷瑙勭濡炲倿宕抽妸銈夊殝闁轰緤鎷�********************************/
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
/****************************闁归鍘ч悾楣冨籍鐠虹儤鐝ら柛鎺擃殙閵嗭拷*************************************************/
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
void ICACHE_FLASH_ATTR  serv_timer_callback()//闁烩偓鍔嬬花顒勫矗閹达讣鎷锋担鍝勑﹂柟顑胯兌缁即寮靛鍛潳闁革綆鐓夌槐婵嬪嫉瀹ュ懎顫ら柛锝冨妺缁绘氨锟芥稒锚缂嶅宕滃鍥﹂柟顒婃嫹
{
	send_serv=1;
	os_timer_disarm(&serv_timer);
	os_printf("send to serv\r\n");
}
/****************************************闁猴拷鐠哄搫鐓傞柡浣哄瀹撲礁顕ｉ敓鑺ユ叏鐎ｎ亶妲遍柣鐑囨嫹*******************************************/
void ICACHE_FLASH_ATTR  pub_timer_callback()
{
	uint8 frist_pos=0;
	uint16 i;
	uint8 shi,fen,miao;
	static uint8 pub_buff[240];		//闁告瑦鍨电粩鐑藉极閻楀牆绁︾紓鍌涙尭閻★拷
	static uint8 state[10];
	static uint8 ip[4]={192,168,1,3};
	os_memset(state,0,os_strlen(state));

	if(pub_flag==1)
	{
		pub_flag=0;
		os_memset(pub_buff,0,os_strlen(pub_buff));
		if(strstr(mqtt_buff,dev_sid)!=NULL)
		{
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_count_down\"")!=NULL)//闁稿﹥甯熼鎼佸籍閿燂拷
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
/****************************闁兼儳鍢茶ぐ鍥磹閹烘洦鍚�闁哄啫澧庢慨鎼佸箑閿燂拷**********************************/
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
/*****************************閻犱礁澧介悿鍡欙拷瑙勭濡烇拷****************************************************/
			//{"cmd":"wifi_socket_timing","day":"1234567","ontime":"10:00","offtime":"19:00","timer":1,"timer_state":"on","sid":"12345678"}
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_timing\"")!=NULL)//閻庤纰嶅鍌炲闯閿燂拷
			{
				frist_pos=GetSubStrPos(mqtt_buff,"\"timer\":");
				if(mqtt_buff[frist_pos+9]>='0'&&mqtt_buff[frist_pos+9]<='9')
				{
					timer=(mqtt_buff[frist_pos+8]-'0')*10+(mqtt_buff[frist_pos+9]-'0');//闁哄牆顦ˇ璺ㄤ焊閹存粓鍤嬮悗瑙勭濡烇拷
				}
				else
					timer=(mqtt_buff[frist_pos+8]-'0');//闁哄牆顦ˇ璺ㄤ焊閹存粓鍤嬮悗瑙勭濡烇拷

				/******************************閻庢稒锚閻ｉ箖寮捄鍝勭仚閻炴冻鎷�******************************************/
				//闁兼儳鍢茶ぐ鍥╋拷瑙勭濡炲倿寮导鏉戞闁挎稑鑻々褔寮稿鍐︿海濞存粣鎷�20濞戞搩浜风槐婵嗐�掗崨顖楁晞鐟滅増鎸告晶鐘绘儍閸曨偆鏆伴柡鍐啇缁辨繃绋夋繝浣虹倞閻℃帒鎳撶换锟�20
				if(get_timer()>19&&timer==0)
				{
					os_memset(pub_buff,0,os_strlen(pub_buff));
					os_sprintf(pub_buff,"{\"cmd\":\"wifi_socket_timeout_ack\",\"sid\":\"%s\"}",dev_sid);
				}
				else
				{
					if(timer==0)//濞寸媴缍�閵嗗啴寮弶璺ㄧ处闁汇劌瀚悾楣冨籍鐠虹儤鐝�
					{
						for(i=1;i<24;i++)
						{
							if(strstr(wifi_socket_timing[i],"time")==NULL)
							{
								timer=i;//濞ｅ洦绻傞悺銊╁棘閺夎法绱﹂悗瑙勭濡炲倿鎯冮崟顏嗙Т缂傚喛鎷�
								break;
							}
						}
					}
					frist_pos=GetSubStrPos(mqtt_buff,"\"day\":");
					os_strncpy(timing_day[timer],mqtt_buff+frist_pos+7,7);//濞ｅ洦绻傞悺銊х箔閻炲绋夐鍕毎闁哄啫澧庡▓鎴︽煂瀹ュ拋妲婚柣銊ュ閵囧寮敓锟�

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
					/*************************************鐎殿噯鎷烽柛姘煎灠閻ｉ箖寮張鍨床闁告棑鎷�*******************************************/
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
/************************************閻犲洩顕ч悾楣冨籍鐠哄搫鐏欓悶娑虫嫹*****************************************************/
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
/*****************************************闁告帞濞�濞呭海锟借纰嶅锟�*****************************************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_del_timing\"")!=NULL)
			{
				frist_pos=GetSubStrPos(mqtt_buff,"\"timer\":");
				if(mqtt_buff[frist_pos+9]>='0'&&mqtt_buff[frist_pos+9]<='9')
				{
					timer=(mqtt_buff[frist_pos+8]-'0')*10+(mqtt_buff[frist_pos+9]-'0');//
				}
				else
					timer=(mqtt_buff[frist_pos+8]-'0');//闁兼儳鍢茶ぐ鍥╂啺娴ｇ鐏╅梻鍕╁�楀▓鎴狅拷瑙勭濡烇拷
				/****************************闁稿繈鍔嶇粩鑽ょ矚妤︽鍤夐悗瑙勭濡炲倿宕抽妸褎鐣遍柣妯垮煐閿熸枻鎷�******************************************/
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
/************************************閻犲洩顕х槐鎴﹀礂瀹曞洤笑闁诡剨鎷�************************************************************/
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
/**********************闁兼儳鍢茶ぐ鍢擯*************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_equipment_ping\"")!=NULL)
			{
				os_sprintf(pub_buff,"{\"cmd\":\"wifi_equipment_ping_ack\",\"ip\":\"%s\",\"sid\":\"%s\"}",local_ip,dev_sid);
				MQTT_Publish(&mqttClient,  pub_topic,pub_buff, os_strlen(pub_buff), 0, 0);
			}
			/****************婵炴挸鎳愰埞鏍拷瑙勭濡炲倿宕氬Δ鍕╋拷锟�*************************/
			if(strstr(mqtt_buff,"\"cmd\":\"wifi_socket_clear\"")!=NULL)
			{
				if(spi_flash_erase_sector(CFG_LOCATION + 5)==SPI_FLASH_RESULT_OK)
				{
					system_restart();
				}
			}
			/**********************鐎殿噯鎷烽柛蹇ユ嫹**************************/
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
			//***********闁告碍鍨跺﹢鍥礉閳ヨ櫕鐝ら柨娑虫嫹3S闁告艾楠歌ぐ鍌炴焻娴ｈ娈堕柟璇″枤缁即寮靛鍛潳闁革絻鍔嬬换姘憋拷娑櫳戦弳鐔煎箲椤曞棛绀夐弶鈺冨仧閻㈣崵鎲撮敃锟借ぐ淇嘍P鐎殿噯鎷烽柛蹇撳暱閹鏁嶇仦鑲╃獥濞戞挸绉甸弻鍥礆闁垮鐓�婵炴挸鎳樺ù鍌滐拷瑙勭濡炲倿宕抽敓锟�*************************/
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
/********************************闂佹澘绉剁紞澶愬箰閸モ斂浠涢柣蹇ｅ灦濡垶鎮滄担鍛婄閻犲鍟崵閬嶅极閿燂拷**********************************************/
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
	wifi_softap_dhcps_stop();//閻犱礁澧介悿鍡涘礈瀹ュ懎褰犻梻鍌滄嫃HCP
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
 * 闁告垼濮ら弳鐔煎触閿燂拷:void Wifi_AP_Init()
 * 闁告梻鍠曢崗姒砳fi_ap闁告帗绻傞～鎰板礌閿燂拷
 */
void ICACHE_FLASH_ATTR WIFIAPInit()
{
    struct softap_config apConfig;

    /***************************婵☆垪锟藉磭纭�閻犱礁澧介悿锟�************************************/
         if(wifi_set_opmode(0x03)){          //  閻犱礁澧介悿鍡樼▔缁″湧婵☆垪锟藉磭纭�

         }else{

         }
    /***************************闁告艾绉撮悺褏鎷嬫ィ鍐挎嫹濮樻湹澹曠紓鍐挎嫹************************************/
	  os_bzero(&apConfig, sizeof(struct softap_config));
	  wifi_softap_get_config(&apConfig);
	  apConfig.ssid_len=0;                      //閻犱礁澧介悿鍞杝id闂傦拷閸喖顔�
	  os_memset(apConfig.ssid,' ',strlen(apConfig.ssid));

	  os_sprintf(apConfig.ssid,"grasp_socket-%s",dev_sid);			//閻犱礁澧介悿鍞杝id闁告艾绉撮悺锟�

	 // os_strcpy(apConfig.password,"12345678");  //閻犱礁澧介悿鍡欙拷闈涙閻栵拷
	 // apConfig.authmode =3;                     //閻犱礁澧介悿鍡涘礉閻樿尙妲曟俊顖楋拷宕囩
	  wifi_softap_set_config(&apConfig);        //闂佹澘绉堕悿锟�

	  dhcps_lease();
}
//闂傦拷閹稿骸鐦婚柟绋款樀閺侇厼顕ｉ敓鑺ユ叏鐎ｎ喖甯崇紓鍐挎嫹
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
/****************************闂佹澘绉剁紞澶愬箰閸モ斂浠涢柣蹇ｅ灠缁辨垶鎱ㄧ�ｎ喗锛忛柣鎴嫹*****************************************/
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

//闁活収鍘界�垫粌顕ｉ敓浠嬪触閿燂拷/闁哄偆鍘肩槐鎴濐嚕閿熶粙宕楅敓锟�
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
	 //闁圭顦甸弫顓㈡煀瀹ュ洨鏋�
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


闁兼儳鍢茶ぐ鍥╋拷娑欘殘椤戜焦绋夐煫顓″幀闁哄本鍔掗柌婊咃拷娑欏姇閻⊙呯箔閿旇儻顩柣銊ュ椤╄崵锟芥稒顨嗛惁婵嬫儍閸曨亞鐟撻柡宥忔嫹

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
 闁告帗绻傞～鎰板礌閿燂拷
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

/*************************************闁兼挳鏅茬粭鍊僫fi闁告艾瀛╅ˉ鍛规繝鐠�*******************************************/
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
			 my_sntp_init();//闁兼儳鍢茶ぐ鍥╃磾閹寸姷鎹曢柡鍐ㄧ埣濡拷
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
	/***************************閻庣數鍘ч崣鍞俵ash闁轰胶澧楀畵渚�鐛幆闈舵帡寮搁敓锟�****************************************/
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
			os_strncpy(wifi_socket_timing[data],buff,frist_pos+1);//闁规惌浜滆ぐ鍥ㄧ▔閿熶粙宕犻崨顔芥闁瑰湱鏅�"time":"10:00,16:00,0034560,on","timer":2}

			os_strncpy(timing_ontime[data],wifi_socket_timing[data]+9,5);
			os_strncpy(timing_offtime[data],wifi_socket_timing[data]+15,5);
			os_strncpy(timing_day[data],wifi_socket_timing[data]+21,7);
			os_strncpy(timing_timersta[data],wifi_socket_timing[data]+29,3);

			os_memset(cache,0,sizeof(cache));
			os_strncpy(cache,buff+frist_pos+2,os_strlen(buff)-frist_pos-2);//閻忓繐妫欓崺鍛村矗閺嶎偅鐣遍柡浣哄瀹撲胶锟芥稒锚濠�鐚歛che濞戞搫鎷�
			os_memset(buff,0,sizeof(buff));//婵炴挸鎳愰埞鏈緐ff
			os_strcpy(buff,cache);//閻忓繐妫欓崺鍛村矗閺嵮冪厒闁汇劌瀚弳鐔煎箲椤曪拷閸ｆ悂寮悧鍫濐伓閻犳劖绻傞崺瀹恥ff
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
	/***************************鐎殿噯鎷烽柛姘煎灟閹广垽宕濋敓锟�**********************************/
	os_timer_disarm(&pub_timer);
	os_timer_setfn(&pub_timer, (os_timer_func_t *)pub_timer_callback, NULL);
	os_timer_arm(&pub_timer, 200, 1);//200ms

}
/* Create a bunch of objects as demonstration. */

void user_init(void)
{
	uint16 i;
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
	//spi_flash_read((CFG_LOCATION + 5) * SPI_FLASH_SEC_SIZE,(uint32 *)list, sizeof(list));
	spi_flash_read(0x78000,(uint32 *)list, sizeof(list));
#if 0
	for(i=0;i<sizeof(list);i++)
		os_printf("list[%d]=%x\r\n",i,list[i]);
#endif
 //婵☆偓鎷锋繛鏉戭儏閸╁本娼婚悙鏉戝ip濞戞柨顑呴幃妤佹交閻愭潙澶峬qtt
	check_ip();

	wifi_set_sleep_type(MODEM_SLEEP_T);

   	system_init_done_cb(to_scan);
	INFO("\r\nSystem started ...\r\n");
	os_printf("SYS_Ver is %s,HARD_Ver is %s\r\n",SYS_VER,HARD_VER);
	os_printf("devsid is %s\r\n",dev_sid);
}



