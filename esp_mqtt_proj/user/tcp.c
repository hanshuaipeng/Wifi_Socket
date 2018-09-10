#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"
#include "tcp.h"
#include "user_config.h"
typedef enum
{
  teClient,
  teServer
}teType;
typedef struct
{
    BOOL linkEn;
  BOOL teToff;
    uint8_t linkId;
    teType teType;
    uint8_t repeaTime;
    uint8_t changType;
    uint8 remoteIp[4];
    int32_t remotePort;
    struct espconn *pCon;
}linkConType;

typedef struct
{
  BOOL linkEn;
  BOOL teToff;
  uint8_t linkId;
  teType teType;
  uint8_t repeaTime;
  struct espconn *pCon;
} espConnectionType;

linkConType pLink;
espConnectionType user_contype;
static struct espconn *pTcpServer;

extern uint8 mqtt_buff[200];
extern uint8 dev_sid[15];
extern uint8 connect_sta;


extern uint8 pub_flag;

uint8 tcp_send=0;
//os_event_t    procTaskQueue[procTaskQueueLen];
/*
 * ������:void Wifi_AP_Init()
 * ����wifi_ap��ʼ��
 */
void WIFIAPInit()
{
    struct softap_config apConfig;

    /***************************ģʽ����************************************/
         if(wifi_set_opmode(0x03)){          //  ����ΪAPģʽ

         }else{

         }
    /***************************������ͨ����************************************/
	  os_bzero(&apConfig, sizeof(struct softap_config));
	  wifi_softap_get_config(&apConfig);
	  apConfig.ssid_len=0;                      //����ssid����
	  os_memset(apConfig.ssid,' ',strlen(apConfig.ssid));

	  os_sprintf(apConfig.ssid,"grasp_socket-%s",dev_sid);			//����ssid����

	 // os_strcpy(apConfig.password,"12345678");  //��������
	 // apConfig.authmode =3;                     //���ü���ģʽ
	  wifi_softap_set_config(&apConfig);        //����

	  dhcps_lease();
}
/*
 *������:void TcpServer_Listen_Recv(void *arg, char *pdata, unsigned short len)
 *����:���ռ�������
 */
void TcpServer_Listen_Recv(void *arg, char *pdata, unsigned short len)
{
	static uint8 pos,i,ssid_len,password_len;
	uint8 ssid[20]={},password[20]={},ack[50]={};
//#if debug
    os_printf("�յ�PC���������ݣ�%s\r\n",pdata);//���ͻ��˷����������ݴ�ӡ����
//endif
    connect_sta=0;
    /*os_sprintf(ack,"{\"cmd\":\"wifi_config_ok\",\"sid\":%x,\"connect_sta\":%d}",sid,connect_sta);
    WIFI_TCP_SendNews(ack,os_strlen(ack));
*/
	if(strstr(pdata,"\"wifi_config\"")!=NULL)
	{
		/****************��ȡssid*********************/
		pos=GetSubStrPos(pdata,"\"ssid_len\"");
		ssid_len=pdata[pos+11]-'0';
		if(pdata[pos+12]>='0'&&pdata[pos+12]<='9')
		{
			ssid_len=ssid_len*10+(pdata[pos+12]-'0');
		}
		else
			ssid_len=ssid_len;
		pos=GetSubStrPos(pdata,"\"ssid\"");
		for(i=0;i<ssid_len;i++)
		{
			ssid[i]=pdata[pos+i+8];
		}
		/****************��ȡpassword*********************/
		pos=GetSubStrPos(pdata,"\"password_len\"");
		password_len=pdata[pos+15]-'0';
		if(pdata[pos+16]>='0'&&pdata[pos+16]<='9')
		{
			password_len=password_len*10+(pdata[pos+16]-'0');
		}
		else
			password_len=password_len;
		pos=GetSubStrPos(pdata,"\"password\"");
		for(i=0;i<password_len;i++)
		{
			password[i]=pdata[pos+i+12];
		}
		/*os_memset(ssid_buff,0,os_strlen(ssid_buff));
		os_memset(password_buff,0,os_strlen(password_buff));
		os_memcpy(ssid_buff,ssid,os_strlen(ssid));
		os_memcpy(password_buff,password,os_strlen(password));
*/
		 wifi_station_disconnect();
		 WIFI_Connect(ssid,password,wifiConnectCb);

	}
	else
	{
		os_strcpy(mqtt_buff,pdata);
		pub_flag=1;
		tcp_send=1;
	}
}
/*
 * ������:void TcpServer_Listen_Recb(void *arg, sint8 errType)
 * ����:���Ӽ�������
 */
void TcpServer_Listen_recon_cb(void *arg, sint8 errType)
{

    struct espconn *pespconn = (struct espconn *)arg;
      linkConType *linkTemp = (linkConType *)pespconn->reverse;
}
/*
 * ������:void Tcp_Server_Listen_discon_cb(void *arg)
 * ����:�����Ͽ�ʱ��������
 */
void Tcp_Server_Listen_discon_cb(void *arg)
{
      struct espconn *pespconn = (struct espconn *) arg;
      linkConType *linkTemp = (linkConType *) pespconn->reverse;
//#if debug
      os_printf("�����Ѿ��Ͽ�\r\n");
//#endif
}
/*
 * ������:void Tcp_Server_Listen_sent_cb(void *arg)
 * ����:���ͳɹ���������
 */
void Tcp_Server_Listen_sent_cb(void *arg)
{
      struct espconn *pespconn = (struct espconn *) arg;
     linkConType *linkTemp = (linkConType *) pespconn->reverse;
//#if debug
     os_printf("�������ݳɹ�����\r\n");
//#endif
}
/*
 * ������:void TcpServer_Listen_PCon(void *arg)
 * ����:�ֻ�����AP��������
 */
void TcpServerListen_PCon(void *arg)
{

    struct espconn *pespconn = (struct espconn *)arg;
      pLink.teToff = FALSE;
      pLink.linkId = 1;
      pLink.teType = teServer;
      pLink.repeaTime = 0;
      pLink.pCon = pespconn;
      pespconn->reverse = &pLink;
      espconn_regist_recvcb(pespconn, TcpServer_Listen_Recv);                           //ע����ռ�������
      espconn_regist_reconcb(pespconn, TcpServer_Listen_recon_cb);                      //ע�����Ӽ�������
      espconn_regist_disconcb(pespconn, Tcp_Server_Listen_discon_cb);                   //ע�������Ͽ�ʱ��������
      espconn_regist_sentcb(pespconn, Tcp_Server_Listen_sent_cb);                       //ע�ᷢ�ͳɹ���������
#if debug
      os_printf("�����Ѿ��ɹ�\r\n");
#endif
}
/*
 * ������:void dhcps_lease()
 * ����:����ip��Χ
 */
void dhcps_lease(void)
{

	struct	dhcps_lease	dhcp_lease;
	struct ip_info info;
	wifi_softap_dhcps_stop();//����ǰ�ر�DHCP
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
 * ������:void WIFI_Server_MODE()
 * ����:���÷�����ģʽ
 */
void WIFIServerMode()
{
	espconn_tcp_set_max_con(5);                                         //����TCP���ӵ�������
	pTcpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	pTcpServer->type = ESPCONN_TCP;                                     //TCP����
	pTcpServer->state = ESPCONN_NONE;                                   //״̬
	pTcpServer->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	pTcpServer->proto.tcp->local_port = 8888;                           //�˿ں�
	espconn_regist_connectcb(pTcpServer, TcpServerListen_PCon);
	espconn_accept(pTcpServer);
	espconn_regist_time(pTcpServer, 180, 0);                            //���ó�ʱ�Ͽ�ʱ�� ��λs
}
/*
 * ������:void WIFI_TCP_SendNews(unsigned char *dat)
 * ����:��TCP������������Ϣ
 */
void WIFI_TCP_SendNews(unsigned char *dat,uint16 len)
{
    espconn_send(pLink.pCon,dat,len);
}


void ICACHE_FLASH_ATTR station_server_init(struct ip_addr *local_ip,int port){
    LOCAL struct espconn esp_conn;

    espconn_tcp_set_max_con(5);                                         //����TCP���ӵ�������

    esp_conn.type=ESPCONN_TCP;
    esp_conn.state=ESPCONN_NONE;
    esp_conn.proto.tcp=(esp_tcp *)os_malloc(sizeof(esp_tcp));

    os_memcpy(esp_conn.proto.tcp->local_ip,local_ip,4);
    esp_conn.proto.tcp->local_port=port;


    //ע�����ӳɹ��Ļص�����������ʧ���������ӵĻص�����
    espconn_regist_connectcb(&esp_conn,TcpServerListen_PCon);//ע��һ�����ӳɹ��ص�����

    espconn_accept(&esp_conn);
}