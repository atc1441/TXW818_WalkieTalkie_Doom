#ifndef __SDK_PROJECT_CONFIG_H__
#define __SDK_PROJECT_CONFIG_H__



#define CUSTOMER_ID 1
#define DEFAULT_SYS_CLK   				(240*1000000) 

#define USB_EN                          0
#define USB_HOST_EN                     0
#define MACBUS_USB
#define USB_DEVICE_MASS_OR_UVC        		//使用rtt device架构需将此宏注释
#define USBDISK                         0   //1代表将sd卡作为u盘   2代表将flash作为usb盘,需要配合USB_EN使用,并且其他宏不能有冲突

/*=========== RTT USB架构宏定义 ==========*/
#define RTT_USB_EN                      0	//RTT USB 架构使能 (USB_EN打开)
#define USB_DETECT_EN					0	//USB 主从检测使能 (USB_HOST_EN关闭、USB_EN打开)

/*            USB HOST            */

//#define RT_USBH						//RTT USB HOST 使能

// #define RT_USBH_CDC
// #define RT_USBH_UVC
// #define RT_USBH_UAC
// #define RT_USBH_WIRELESS
// #define RT_USBH_VENDOR_QUECTEL
// #define RT_USBH_VENDOR_CHINAMOBILE
// #define RT_USBH_MSTORAGE

/*           USB DEVICE           */

//#define RT_USING_USB_DEVICE			//RTT USB DEVICE 使能

//#define RT_USB_DEVICE_COMPOSITE       //USB DEVICE 复合设备使能

// #define RT_USB_DEVICE_CDC
// #define RT_USB_DEVICE_AUDIO_MIC
// #define RT_USB_DEVICE_AUDIO_SPEAKER
// #define RT_USB_DEVICE_MSTORAGE
// #define RT_USB_DEVICE_VIDEO
// #define RT_USB_DEVICE_RNDIS
// #define RT_USB_DEVICE_HID
// #define RT_USB_DEVICE_WINUSB

/*================== end =================*/

/*============== WPA3宏定义 ==============*/
#define WIFI_MODULE_80211W_EN   1
#define CONFIG_IEEE80211W
#define CONFIG_SAE
#define CONFIG_OWE
#define WPA_CRYPTO_OPS 1

#define BLE_PAIR_NET                    0
#define WIRELESS_PAIR_CODE              0

#define PRC_EN                          1
#define OF_EN                           1
#define DVP_EN                          1
#define VPP_EN                          DVP_EN
#define JPG_EN                         (1 && DVP_EN)
#define LCD_EN			 				1
#define SCALE_EN						1
#define SDH_EN                          0
#define FS_EN                           1
#define SD_SAVE                         (0&&SDH_EN&&FS_EN&&JPG_EN)

#define VCAM_EN                        (0 || DVP_EN)

#define OPENDML_EN                      1
#define UART_FLY_CTRL_EN                0
#define PWM_EN                          0
#define KEY_MODULE_EN					1
#define FLASHDISK_EN					1
#define MP3_EN                          0
#define AMR_EN                          0
#define NET_PAIR                        1
#define PRINTER_EN						0


#define AUDIO_EN                        1
#define AUDIO_DAC_EN                    1
#define RTP_SOUND                       (1&&AUDIO_EN)

#define MJPEG_VIDEO                     (1 &&OPENDML_EN&&FS_EN&&SDH_EN&&JPG_EN)          //基于框架的mjpeg录像    
#define UVC_VIDEO                       (1 &&OPENDML_EN&&FS_EN&&SDH_EN&&USB_EN)          //基于框架的uvc录像

#define DCDC_EN                         1           //板子是否使用DCDC电路
//低功耗demo
#define LOWPOWER_DEMO                   0

///////////////wifi parameter////////////
#define WIFI_RF_PWR_LEVEL               0           //选择WIFI功率。0：普通功率；1：比0的功率小；2：比1更小；10：大电流
#define WIFI_RTS_THRESHOLD              1600        //RTS阈值，2304等效于不用RTS
#define WIFI_RTS_MAX_RETRY              2           //RTS重试次数，范围为2~16
#define WIFI_TX_MAX_RETRY               7           //最大传输次数，范围为1~31
#define WIFI_TX_MAX_POWER               7           //TX最大发射功率，有0~7档
/* 每1bit代表一种速率。每bit代表的格式：
 * bit 0  : DSSS 1M
 * bit 1  : DSSS 2M
 * bit 2  : DSSS 5.5M
 * bit 3  : DSSS 11M
 * bit 4  : NON-HT 6M
 * bit 5  : NON-HT 9M
 * bit 6  : NON-HT 12M
 * bit 7  : NON-HT 18M
 * bit 8  : NON-HT 24M
 * bit 9  : NON-HT 36M
 * bit 10 : NON-HT 48M
 * bit 11 : NON-HT 54M
 * bit 12 : HT MCS0
 * bit 13 : HT MCS1
 * bit 14 : HT MCS2
 * bit 15 : HT MCS3
 * bit 16 : HT MCS4
 * bit 17 : HT MCS5
 * bit 18 : HT MCS6
 * bit 19 : HT MCS7
 */
#define WIFI_TX_SUPP_RATE               0x0FFFFF    //TX速率支持，每1bit对应一种速率
#define WIFI_MAX_STA_CNT                8           //最多连接sta的数量。有效值为1~8
#define WIFI_MULICAST_RETRY             0           //组播帧传输次数
#define WIFI_ACS_CHAN_LISTS             0x03FF      //要扫描的信道。每1bit对应1个信道(bit 0~11 -> chan 1~12)
#define WIFI_ACS_SCAN_TIME              150         //每个信道的扫描时间，单位ms
#define WIFI_MAX_PS_CNT                 30           //底层为休眠sta缓存的帧最大数量。0代表sta休眠由umac全程管理，底层不缓存
#define CHANNEL_DEFAULT                 0
#define SSID_DEFAULT                    "HG-WIFI_"
#define WIFI_TX_DUTY_CYCLE              80         //tx发送占空比，单位是%，范围是0~100
#define WIFI_SSID_FILTER_EN             0           //是否使能SSID过滤功能。使能后，只有隐藏SSID和指定SSID的beacon才会上传
#define WIFI_PREVENT_PS_MODE_EN         1           //是否尽可能的阻止sta进入休眠
#define IP_ADDR_DEFAULT                 0x0101A8C0  //192.168.1.1
#define NET_MASK_DEFAULT                0x00FFFFFF  //255.255.255.0
#define GW_IP_DEFAULT                   0x0101A8C0  //192.168.1.1
#define DHCP_START_IP_DEFAULT           0x6401A8C0  //192.168.1.100
#define DHCP_END_IP_DEFAULT             0xFE01A8C0  //192.168.1.254
///////////////uart1////////////
#define PIN_UART0_TX PC_7//PC_6//PA_9
#define PIN_UART0_RX 255//PC_6
#define ATCMD_UARTDEV       HG_UART0_DEVID


///////////////uart4////////////
#define PIN_UART4_TX 255
#define PIN_UART4_RX 255

#define PIN_PDM_MCLK PA_4
#define PIN_PDM_DATA PA_3

#define PIN_IIS0_MCLK 255
#define PIN_IIS0_BCLK PA_7
#define PIN_IIS0_WCLK PA_8
#define PIN_IIS0_DATA PA_5


#define LVGL_STREAM_ENABLE  0
///////////////LCD/////////////
#define LCD_nv3031a_MCU565_EN 1


#define PIN_LCD_RESET  PC_15
#define PIN_LCD_RESET1  PC_6

#define PIN_SPI0_CS  PA_8
#define PIN_SPI0_CLK PA_2
#define PIN_SPI0_IO0 PA_0
#define PIN_SPI0_IO1 255 
#define PIN_SPI0_IO2 255
#define PIN_SPI0_IO3 255

#define DE_ERD PA_2
#define VS_CS      PC_14
#define HS_DC      PC_13
#define DOTCLK_RWR PC_12
#define LCD_TE     255

#define LCD_D0     PC_11
#define LCD_D1     PC_10
#define LCD_D2     PC_9
#define LCD_D3     PC_8
#define LCD_D4     PA_13
#define LCD_D5     PA_12
#define LCD_D6     PC_4
#define LCD_D7     PC_5
#define LCD_D8 255
#define LCD_D9 255
#define LCD_D10 255
#define LCD_D11 255
#define LCD_D12 255
#define LCD_D13 255
#define LCD_D14 255
#define LCD_D15 255
#define LCD_D16 255
#define LCD_D17 255
#define LCD_D18 255
#define LCD_D19 255
#define LCD_D20 255
#define LCD_D21 255
#define LCD_D22 255
#define LCD_D23 255

#define SDH_I2C2_REUSE    (0 && DVP_EN && SDH_EN)

/*---------------------------------------*/
/*---------SDH PIN DEFINITION------------*/
/*---------------------------------------*/

#define PIN_SDH_CLK		  PC_4//PC_2//
#define PIN_SDH_CMD		  PC_3//PC_3//
#define PIN_SDH_DAT0	  PC_2//PC_4//
#define PIN_SDH_DAT1	  255
#define PIN_SDH_DAT2	  255
#define PIN_SDH_DAT3	  255



///////////////IIC2/////////////
#define PIN_IIC2_SCL PC_2//PA_11//
#define PIN_IIC2_SDA PC_3//PA_8//

#define PIN_IIC1_SCL 255
#define PIN_IIC1_SDA 255





#define  MOTO_AIN1_PIN	 PB_7
#define  MOTO_AIN2_PIN	 PB_6
#define  MOTO_BIN1_PIN	 PC_0
#define  MOTO_BIN2_PIN	 PC_1


#define PIN_PWM_CHANNEL_0 255
#define PIN_PWM_CHANNEL_1 255
#define PIN_PWM_CHANNEL_2 255
#define PIN_PWM_CHANNEL_3 255
#define PIN_PWM_CHANNEL_4 255


#define PIN_SPI_PDN						0


///////////////DVP//////////////
#define CMOS_AUTO_LOAD   1
#define DEV_SENSOR_GC0308 1
#define DEV_SENSOR_GC0309 1
#define DEV_SENSOR_GC0311 1
#define DEV_SENSOR_GC0312 1
#define DEV_SENSOR_GC0328 1
#define DEV_SENSOR_GC0329 1
#define DEV_SENSOR_BF3A03 1
#define DEV_SENSOR_BF3703 1
#define DEV_SENSOR_OV2640 1
#define DEV_SENSOR_OV7725 1
#define DEV_SENSOR_OV7670 1
#define DEV_SENSOR_OV7660 1
#define DEV_SENSOR_BF2013 1
#define DEV_SENSOR_XC7016_H63 1
#define DEV_SENSOR_XC7011_H63 1
#define DEV_SENSOR_XC7011_GC1054 1
#define DEV_SENSOR_XCG532 1
#define DEV_SENSOR_SP0718 1
#define DEV_SENSOR_SP0A19 1
#define DEV_SENSOR_BF3720 1
#define DEV_SENSOR_GCcustom 1

#define PIN_DVP_RESET  255//PA_12//
#define PIN_DVP_PDN    PA_8


//////////////SET QC/////////////////////////////
#define NET_TO_QC         0
#define QC_STA_NAME       "BGN_test"
#define QC_STA_PW         "12345678"
#define QC_SCAN_TIME	  200      //ms
#define QC_SCAN_CNT	      10       //total time = QC_SCAN_TIME*QC_SCAN_CNT
#define QC_SCAN_CHANNEL   0x010      //要扫描的信道。每1bit对应1个信道(bit 0~11 -> chan 1~12)



//////////////////////////////////////////////////jpg app room cfg////////////////////////////////////////////////////
#define JPG_HEAD_RESERVER  0       //每段JPEG BUF前面预留用于填充其他数据的数据量，无需预留填0
#define JPG_BUF_LEN        1308//2880//4096+24   // 1*1024// 1*1024//       JPG buf长度+reserver长度          1308
#define JPG_NODE           40//18//30*2
#define JPG_TAIL_RESERVER  0

#define JPG0_HEAD_RESERVER  0//0       //每段JPEG BUF前面预留用于填充其他数据的数据量，无需预留填0
#define JPG0_BUF_LEN        1400//8192//1308//2880//4096+24   // 1*1024// 1*1024//       JPG buf长度+reserver长度          1308
#define JPG0_NODE           40//20//18//30*2
#define JPG0_TAIL_RESERVER  0

//////////////////////////////////////////////////jpg app room cfg////////////////////////////////////////////////////
#define JPG1_HEAD_RESERVER  0       //每段JPEG BUF前面预留用于填充其他数据的数据量，无需预留填0
#define JPG1_BUF_LEN        1308//2880//4096+24   // 1*1024// 1*1024//       JPG buf长度+reserver长度          1308
#define JPG1_NODE           40//18//30*2
#define JPG1_TAIL_RESERVER  0


//设定自定义空间,前提heap要足够,main的时候会从heap申请这样一大片空间分配到custom_malloc中
//空间组成大概是  jpg空间+csi空间(VGA 30kB)+ 其他模块的空间
//系统尽量留有20K左右,防止其他地方malloc申请不到

//#define VCAM_33


//#define WIFI_BRIDGE_EN  1
//#define WIFI_BRIDGE_DEV HG_GMAC_DEVID

#define CUSTOM_PSRAM_SIZE   1500*1024  /* DOOM runtime heap: small_doom.bin (1.4MB static) + game data */
#define PSRAM_HEAP			2200*1024//如果需要psram当作heap,需要打开这个宏

//速率调整参数选择，默认是耳勺
#define RATE_CONTROL_ERSHAO         1
#define RATE_CONTROL_HANGPAI        2
#define RATE_CONTROL_IPC            3
#define RATE_CONTROL_BABYMPNITOR    4

#define RATE_CONTROL_SELECT     RATE_CONTROL_BABYMPNITOR

#if (RATE_CONTROL_SELECT == RATE_CONTROL_BABYMPNITOR)
//#define WIFI_TX_MAX_RETRY               5           //最大传输次数，范围为1~31
#endif

//是能5m/20m共存和自动带宽切换，
//#define WIFI_FEM_CHIP		LMAC_FEM_GSR2401C
//#define LMAC_BGN_PCF
			
///////////////ble enable////////////
#define BLE_SUPPORT			0
#define BLE_METER_TEST_EN   0

#define SYS_APP_UHTTPD		1
#define WEB_DISABLE_AUTH	1
#define ALT_LITE_WEB		1

#endif

