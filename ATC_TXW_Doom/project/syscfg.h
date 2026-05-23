#ifndef _PROJECT_SYSCFG_H_
#define _PROJECT_SYSCFG_H_

#include "lib/umac/ieee80211.h"

#define CFG_VERSION_NUM  0x0103

enum WIFI_WORK_MODE {
    WIFI_MODE_NONE = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
    WIFI_MODE_P2P,
};



struct system_status {
    uint32 dhcpc_done: 1,
           wifi_connected: 1,
           dbg_heap: 1,
           dbg_top: 2,
           dbg_lmac: 1,
           dbg_umac: 1,
           dbg_irq: 1,
           reset_wdt: 1,
           upgrading: 1,
           pair_success: 1,
		   ntp_update: 1;    
    int8   rssi;
    int8   evm;
    uint8  channel;
    uint8  bssid[6];
    uint8  pair_peer[6];
    uint8  wifi_mode;
    uint8  wifi_status_code;
    struct {
        uint32 ipaddr, netmask, svrip, router, dns1, dns2;
    } dhcpc_result;
};

struct sys_config {
    uint16 magic_num, crc;
    uint16 size, cfg_ver, rev2;
    ///////////////////////////////////////
    uint8  wifi_hwmode, default_wifi_mode;
    uint8  wifi_mode, bss_bw, tx_mcs, channel;
    uint8  bssid[6], mac[6];
    uint8  ssid[SSID_MAX_LEN+1];
    uint8  psk[32];
    char   passwd[PASSWD_MAX_LEN+1];
    uint16 bss_max_idle, beacon_int;
    uint16 ack_tmo, dtim_period;
    uint32 key_mgmt;

    uint32 dhcpc_en:1, dhcpd_en:1, ap_hide:1, cfg_init:1, xxxxxx: 28;
    uint32 ipaddr, netmask, gw_ip;
    uint32 dhcpd_startip, dhcpd_endip, dhcpd_lease_time;
    uint32 user_param1,user_param2;
    
    uint32 dsleep_test_cfg[4];
    uint16 wireless_paircode;
    struct ieee80211_bss_wpadata bss_data;
    uint8  ble_pair_status;
    uint8  net_pair_switch;
    uint8  station_channel;
    uint32 dhcpd_dns1, dhcpd_dns2, dhcpd_router;
    uint8  devname[8];
    uint8  web_pwd[PASSWD_MAX_LEN+1];

    uint8  coze_conversation_id[64];
};

extern struct sys_config sys_cfgs;
extern struct system_status sys_status;



void syscfg_set_default_val(void);
void netcfg_flush(void);
int32 wificfg_flush(uint8 ifidx);
void syscfg_flush(int32 reset);
void syscfg_check(void);
void syscfg_dump(void);

uint8 get_sys_cfgs_ble_pair_status();
void wifi_create_station(char *ssid,char *password,int key_mode);
void wifi_create_ap(char *ssid,char *password,int key_mode,int channel);
void syscfg_save_AP_msg(uint8_t *bssid,uint8_t station_channel,struct ieee80211_bss_wpadata *bss_data);
#endif

