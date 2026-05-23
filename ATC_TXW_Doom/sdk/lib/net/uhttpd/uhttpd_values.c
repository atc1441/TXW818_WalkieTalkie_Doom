#include "typesdef.h"
#include "sys_config.h"
#include "list.h"
#include "osal/task.h"
#include "osal/string.h"
#include "osal/sleep.h"
#include "osal/work.h"
#include "lib/common/sysevt.h"
#include "lib/common/common.h"
#include "lib/syscfg/syscfg.h"
#include "lib/net/uhttpd/uhttpd.h"
#include "lib/umac/ieee80211.h"
#include "lib/net/dhcpd/dhcpd.h"
#include "syscfg.h"
#include "dev.h"
#include "lib/net/ethphy/eth_phy.h"
#include "devid.h"

static uint64 uhttpd_auth_token = 0;
static uint64 uhttpd_auth_tick  = 0;

static void uhttpd_get_uptime(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    uint32 day = 0;
    uint32 hour = 0;
    uint32 minute = os_jiffies_to_msecs(os_jiffies()) / 60000;
    if (minute >= 60) {
        if (minute >= 1440) {
            day = minute / 1440;
            hour = minute % 1440 / 60;
            minute = minute % 1440 % 60;
        } else {
            hour = minute / 60;
            minute = minute % 60;
        }
    }
    *resp_len = os_snprintf(resp, size, "\"uptime\":\"%uD %uH %uM\"", day, hour, minute);
}

static void uhttpd_get_fwver(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    int32 i = 0;
    int32 len = 0;
    char *sdk_version = VERSION_STR;
    len += os_sprintf(resp, "\"fwver\":\"");
    for (i = 0; i < os_strlen(sdk_version); i++) {
        if (sdk_version[i] != '*' && sdk_version[i] != '\r' && sdk_version[i] != '\n')
            resp[len++] = sdk_version[i];
    }
    resp[len++] = '"';
    *resp_len = len;
}

static void uhttpd_get_wifi_scanlist(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    int32 i   = 0;
    int32 len = 0;
    int32 ret = 0;
    struct hgic_bss_info *bsslist = os_malloc(sizeof(struct hgic_bss_info) * 32);
    if (bsslist) {
        ret = ieee80211_get_bsslist(bsslist, 32, 1);
        for (i = 0; i < ret && len < size - 128; i++) {
            len += os_snprintf(resp + len, size - len, MACSTR, MAC2STR(bsslist[i].bssid));
            resp[len] = 31; len++;
            len += os_snprintf(resp + len, size - len, "%s", bsslist[i].ssid);
            resp[len] = 31; len++;
            len += os_snprintf(resp + len, size - len, "%d", bsslist[i].encrypt);
            resp[len] = 31; len++;
            len += os_snprintf(resp + len, size - len, "%d", bsslist[i].signal);
            resp[len] = 31; len++;
            len += os_snprintf(resp + len, size - len, "%d", bsslist[i].freq);
            resp[len] = 29; len++;
        }
        if (resp[len -1] == 29) len--;
        os_free(bsslist);
    }
    *resp_len = len;
}

static void uhttpd_get_wifi_stalist(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    int32 i = 0;
    int32 len = 0;
    int32 ret = 0;
    uint32 ip = 0;
    uint8 ifidx = (sys_cfgs.wifi_mode == WIFI_MODE_APSTA ? WIFI_MODE_STA : sys_cfgs.wifi_mode);
    struct hgic_sta_info *stalist = os_malloc(sizeof(struct hgic_sta_info) * 32);
    if (stalist) {
        ret = ieee80211_get_stalist(ifidx, stalist, 32);
        for (i = 0; i < ret && len < size - 128; i++) {
#if SYS_APP_DHCPD
            if (sys_cfgs.dhcpd_en) {
                ip = dhcpd_get_lease_ip(stalist[i].addr);
            }
#endif  
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].aid);
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].ps);
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, MACSTR, MAC2STR(stalist[i].addr));
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, IPSTR, IP2STR_N(ip));
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].rssi);
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].evm);
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].tx_snr);
        resp[len] = 31; len++;
        len += os_snprintf(resp + len, size - len, "%d", stalist[i].rx_snr);
        resp[len] = 29; len++;
        }
        if (resp[len -1] == 29) len--;
        os_free(stalist);
    }
    *resp_len = len;
}

static void uhttpd_get_dhcp_leaselist(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    int32 len = 0;
#if SYS_APP_DHCPD
    int32 i = 0;
    int32 ret = 0;
    struct dhcpd_leaseinfo *leaseslist = os_malloc(sizeof(struct dhcpd_leaseinfo) * 12);
    if (leaseslist) {
        ret = dhcpd_get_lease_list(leaseslist, 12);
        for (i = 0; i < ret && len < size - 64; i++) {
            leaseslist[i].ip = os_ntohl(leaseslist[i].ip);
            len += os_snprintf(resp + len, size - len, MACSTR, MAC2STR(leaseslist[i].mac));
            resp[len] = 31; len++;
            len += os_snprintf(resp + len, size - len, IPSTR, IP2STR_H(leaseslist[i].ip));
            resp[len] = 29; len++;
        }
        if (resp[len - 1] == 29) len--;
        os_free(leaseslist);
    }
#endif
    *resp_len = len;
}

static void uhttpd_get_conn_state(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{   
    int32 len = 0;
    len += os_snprintf(resp + len, size -len, "%d", sys_status.wifi_connected);
    resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", sys_status.rssi);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", sys_status.evm);
	resp[len] = 31; len++;
    len += os_snprintf(resp + len, size - len, "%d", sys_status.channel);
    *resp_len = len;
}

void uhttpd_wifi_scan(void)
{
    uint8 ifidx = (sys_cfgs.wifi_mode == WIFI_MODE_APSTA ? WIFI_MODE_STA : sys_cfgs.wifi_mode);
    struct ieee80211_scandata scan;
    os_memset(&scan, 0, sizeof(scan));
    scan.chan_bitmap = 0xffffffff;
    scan.scan_cnt  = 1;
    scan.scan_time = 100;
    ieee80211_scan(ifidx, 1, &scan);
}

static void uhttpd_save_config(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    uhttpd_auth_tick  = os_jiffies();
    uhttpd_resp_status(sock, resp, 1);
#if WIFI_REPEATER_SUPPORT
    if (sys_cfgs.wifi_mode == WIFI_MODE_APSTA) {
        wpa_passphrase(sys_cfgs.r_ssid, sys_cfgs.r_passwd, sys_cfgs.r_psk);
		sys_cfgs.dhcpc_en = 1;
    }
#endif
    wpa_passphrase(sys_cfgs.ssid, sys_cfgs.passwd, sys_cfgs.psk);
    syscfg_save();
}

static void uhttpd_mcu_reset(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    uhttpd_resp_status(sock, resp, 1);
    os_printf(KERN_ERR"*REBOOT*\r\n");
    os_sleep(1);
    mcu_reset();
}

static void uhttpd_load_default(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    uhttpd_resp_status(sock, resp, 1);
    os_sleep(1);
    syscfg_loaddef();
    mcu_reset();
}
static void uhttpd_apply_cfg(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
    uhttpd_resp_status(sock, resp, 1);
    os_sleep(1);
    syscfg_flush(os_atoi(val));
}


static void uhttpd_device_cfg(int32 sock, char *val, char *resp, int32 size, uint32 *resp_len)
{
	int32 len = 0;
	len += os_snprintf(resp + len, size - len, "%d", sys_cfgs.cfg_init);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", sys_cfgs.wifi_mode);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", WEB_HIDE_DHCPD_CFG ? 0 : SYS_APP_DHCPD);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", FIX_WIFI_MODE);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", WEB_ADVCFG_DISABLE);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", WEB_DISABLE_APSTA);
	resp[len] = 31; len++;
	len += os_snprintf(resp + len, size - len, "%d", WEB_DISABLE_AUTH);
	*resp_len = len;
}


static const struct uhttpd_value_entry uhttpd_values[] = {
    // WiFi Settings
    {.name = "wifi_mode", .value = &sys_cfgs.wifi_mode, .type = UHTTPD_VALUE_TYPE_UINT8},
    {.name = "wifi_keymgmt", .value = &sys_cfgs.key_mgmt, .type = UHTTPD_VALUE_TYPE_UINT32},
    {.name = "wifi_ssid", .value = &sys_cfgs.ssid, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = SSID_MAX_LEN},
    {.name = "wifi_passwd", .value = &sys_cfgs.passwd, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = 32},
    {.name = "wifi_beaconint", .value = &sys_cfgs.beacon_int, .type = UHTTPD_VALUE_TYPE_UINT16},
    {.name = "wifi_dtim_period", .value = &sys_cfgs.dtim_period, .type = UHTTPD_VALUE_TYPE_UINT16},
    {.name = "wifi_bss_max_idle", .value = &sys_cfgs.bss_max_idle, .type = UHTTPD_VALUE_TYPE_UINT16},
    {.name = "wifi_chan", .value = &sys_cfgs.channel, .type = UHTTPD_VALUE_TYPE_UINT8},
    {.name = "wifi_mcs", .value = &sys_cfgs.tx_mcs, .type = UHTTPD_VALUE_TYPE_UINT8},
    {.name = "wifi_bssbw", .value = &sys_cfgs.bss_bw, .type = UHTTPD_VALUE_TYPE_UINT8},
    {.name = "wifi_ap_hide", .value = &sys_cfgs.key_mgmt + 1, .type = UHTTPD_VALUE_TYPE_UINT32_BIT, .bitoff = 4, .maskbit = 0x1},
    // WiFi Repeater
#if WIFI_REPEATER_SUPPORT
    {.name = "wifi_r_ssid", .value = &sys_cfgs.r_ssid, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = SSID_MAX_LEN},
    {.name = "wifi_r_passwd", .value = &sys_cfgs.r_passwd, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = PASSWD_MAX_LEN},
    {.name = "wifi_r_keymgmt", .value = &sys_cfgs.r_key_mgmt, .type = UHTTPD_VALUE_TYPE_UINT32},
#endif
    // LAN Settings
    {.name = "ipaddr", .value = &sys_cfgs.ipaddr, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "netmask", .value = &sys_cfgs.netmask, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "gw_ip", .value = &sys_cfgs.gw_ip, .type = UHTTPD_VALUE_TYPE_IPV4},
    // DHCPC leased IP
    {.name = "s_ipaddr", .value = &sys_status.dhcpc_result.ipaddr, .type = UHTTPD_VALUE_TYPE_IPV4, .flags = UHTTPD_VALUE_FLAGS_READONLY},
    {.name = "s_netmask", .value = &sys_status.dhcpc_result.netmask, .type = UHTTPD_VALUE_TYPE_IPV4, .flags = UHTTPD_VALUE_FLAGS_READONLY},
    {.name = "s_gw_ip", .value = &sys_status.dhcpc_result.router, .type = UHTTPD_VALUE_TYPE_IPV4, .flags = UHTTPD_VALUE_FLAGS_READONLY},
	// DHCP Settings
    {.name = "dhcp_startip", .value = &sys_cfgs.dhcpd_startip, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "dhcp_endip", .value = &sys_cfgs.dhcpd_endip, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "dhcp_lease_time", .value = &sys_cfgs.dhcpd_lease_time, .type = UHTTPD_VALUE_TYPE_UINT32},
    {.name = "dhcp_dns1", .value = &sys_cfgs.dhcpd_dns1, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "dhcp_dns2", .value = &sys_cfgs.dhcpd_dns2, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "dhcp_router", .value = &sys_cfgs.dhcpd_router, .type = UHTTPD_VALUE_TYPE_IPV4},
    {.name = "dhcpd_en", .value = &sys_cfgs.key_mgmt + 1, .type = UHTTPD_VALUE_TYPE_UINT32_BIT, .bitoff = 1, .maskbit = 0x1},
    {.name = "dhcpc_en", .value = &sys_cfgs.key_mgmt + 1, .type = UHTTPD_VALUE_TYPE_UINT32_BIT, .bitoff = 0, .maskbit = 0x1},
	// Syscfg control
    {.name = "savecfg", .value = uhttpd_save_config, .type = UHTTPD_VALUE_TYPE_CMD},
    {.name = "reset", .value = uhttpd_mcu_reset, .type = UHTTPD_VALUE_TYPE_CMD},
    {.name = "loaddef", .value = uhttpd_load_default, .type = UHTTPD_VALUE_TYPE_CMD},
    {.name = "applycfg", .value = uhttpd_apply_cfg, .type = UHTTPD_VALUE_TYPE_CMD},
    // Status
    {.name = "scanap", .value = uhttpd_wifi_scan, .type = UHTTPD_VALUE_TYPE_CMD},
    {.name = "scanlist", .value = uhttpd_get_wifi_scanlist, .type = UHTTPD_VALUE_TYPE_RAW},
    {.name = "stalist", .value = uhttpd_get_wifi_stalist, .type = UHTTPD_VALUE_TYPE_RAW},
    {.name = "conn_state", .value = uhttpd_get_conn_state, .type = UHTTPD_VALUE_TYPE_RAW},
    {.name = "dhcp_leaselist", .value = uhttpd_get_dhcp_leaselist, .type = UHTTPD_VALUE_TYPE_RAW},
    // Sys info
    {.name = "fwver", .value = uhttpd_get_fwver, .type = UHTTPD_VALUE_TYPE_CMD},
    {.name = "uptime", .value = uhttpd_get_uptime, .type = UHTTPD_VALUE_TYPE_CMD},
	// Device Settings
	{.name = "mac_addr", .value = &sys_cfgs.mac, .type = UHTTPD_VALUE_TYPE_MACV4},
	{.name = "web_passwd", .value = &sys_cfgs.web_pwd, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = PASSWD_MAX_LEN},
    {.name = "hostname", .value = &sys_cfgs.devname, .type = UHTTPD_VALUE_TYPE_STRING, .maxlen = 8 },
	// Enabled sys feature
	{.name = "device_cfg", .value = uhttpd_device_cfg, .type = UHTTPD_VALUE_TYPE_RAW},
	// Factory state flag
    {.name = "cfg_init", .value = &sys_cfgs.key_mgmt + 1, .type = UHTTPD_VALUE_TYPE_UINT32_BIT, .bitoff = 3, .maskbit = 0x1}
};

const struct uhttpd_value_entry *uhttpd_find_value(char *name)
{
    int32 i = 0;
    for (i = 0; i < ARRAY_SIZE(uhttpd_values); i++) {
        if (os_strcmp(name, uhttpd_values[i].name) == 0) {
            return &uhttpd_values[i];
        }
    }
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
static const struct uhttpd_action uhttpd_actions[] = {
    {.name = "post.action", uhttpd_proc_post_action},
    {.name = "fwupg.data",  uhttpd_proc_firmware_upg},
    {.name = "auth.action", uhttpd_proc_auth_action},
};

const struct uhttpd_action *uhttpd_action_find(char *name)
{
    int32 i = 0;
    for (i = 0; i < ARRAY_SIZE(uhttpd_actions); i++) {
        if (os_strcmp(name, uhttpd_actions[i].name) == 0) {
            return &uhttpd_actions[i];
        }
    }
    return NULL;
}


int32 uhttpd_check_auth(int32 sock, uint32 client_ip, char *buf, uint32 len)
{
#if WEB_DISABLE_AUTH == 0
    int32 ret = 0;
    char *ptr = os_strstr(buf, "Auth-Token: ");
	if (!ptr)
		ptr = os_strstr(buf, "auth-token: ");
    ret = ((ptr && uhttpd_auth_token) &&
           ((uint64)os_atoll(ptr + 12) + client_ip == uhttpd_auth_token) &&
           (DIFF_JIFFIES(uhttpd_auth_tick, os_jiffies()) < os_msecs_to_jiffies(600 * 1000)));
    return ret;
#else
    return 1;
#endif
}

void uhttpd_proc_auth_action(int32 sock, uint32 client_ip, char *buf, uint32 len)
{
    char *ptr;
    int32 resp_len = 0;
    uint8 pwd[33];
    uint32 token = 0;

    os_random_bytes((uint8 *)&token, 4);
    uhttpd_auth_token = (uint64)token + client_ip;
    uhttpd_auth_tick  = os_jiffies();

    ptr = os_strstr(buf, "\r\n\r\n");
    if (ptr == NULL) {
        return;
    }

    ptr += 4;
    os_memset(pwd, 0, sizeof(pwd));
    hex2bin(ptr, pwd, 32);
#if WEB_DISABLE_AUTH == 0
    if (os_strcmp(pwd, sys_cfgs.web_pwd)) {
        os_printf(KERN_NOTICE"uhttpd auth fail: [%s][%s]\r\n", pwd, sys_cfgs.web_pwd);
        uhttpd_send_resp(sock, 401, 0, 0, 0);
        return;
    }
#endif
    os_memset(pwd, 0, sizeof(pwd));
    resp_len = os_snprintf((char *)pwd, 32, "%u", token);
    uhttpd_send_resp(sock, 200, 0, (char *)pwd, resp_len);
}

