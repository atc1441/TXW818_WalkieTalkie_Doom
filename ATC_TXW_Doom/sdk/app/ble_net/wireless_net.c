#include "typesdef.h"
#include "list.h"
#include "osal/string.h"
#include "syscfg.h"
#include "lib/lmac/lmac.h"
#include "lib/skb/skbuff.h"
#include "lib/ble/ble_adv.h"
#include "lib/ble/ble_def.h"

struct wireless_pair_ctrl{
    struct bt_ops *ops;
};
struct wireless_pair_ctrl *wireless_pair;

int32 wireless_pair_tx(struct bt_ops *ops, uint8 *data, uint32 len)
{
	struct bt_ops *bt_ops = ops;
	struct sk_buff  *skb;

	skb = alloc_tx_skb(len);
	if (bt_ops && skb) {
		hw_memcpy(skb->data, data, len);
		skb->len = len;
		if (bt_ops->tx(bt_ops, skb) == 0) {
			kfree_skb(skb);
			return RET_OK;
		}
		kfree_skb(skb);
	}
	return RET_ERR;
}

static int32 wireless_pair_rx(void *priv, struct sk_buff *skb)
{
    struct bt_rx_info  *rxinfo = (struct bt_rx_info *)skb->cb;
    
    if ((rxinfo->frm_type & 0x80)) {
		dump_hex("ble adv rx adv_data:\r\n", skb->data, skb->len, 1);




    }
    return RET_OK;
}

int32 wireless_pair_init(struct bt_ops *ops)
{
	os_printf("wireless_pair init\r\n");
	uint8 scan_resp[] = {0x04, 0x09, 0x53, 0x53, 0x53, 0x19, 0xFF, 0xD0, 0x07, 0x01, 0x03, 0x00, 0x00, 0x0C, 0x00, 0x88, 0xD1, 0xC4, 0x89, 0x2B, 0x56, 0x7D, 0xE5, 0x65, 0xAC, 0xA1, 0x3F, 0x09, 0x1C, 0x43, 0x92};
	uint8 adv_data[] = {0x02, 0x01, 0x06, 0x03, 0x02, 0x01, 0xA2, 0x14, 0x16, 0x01, 0xA2, 0x01, 0x6B, 0x65, 0x79, 0x79, 0x66, 0x67, 0x35, 0x79, 0x33, 0x34, 0x79, 0x71, 0x78, 0x71, 0x67, 0x64};
	uint8 dev_addr[] = {0x00, 0x12, 0x34, 0x56, 0x78, 0x00};

	if (wireless_pair == NULL) {
		wireless_pair = (struct wireless_pair_ctrl *)os_zalloc(sizeof(struct wireless_pair_ctrl));
	}
	if (ops) {
        wireless_pair->ops = ops;
        ops->rx = wireless_pair_rx;
        ops->priv = wireless_pair;
    }

	ble_ll_set_devaddr(ops, dev_addr);
	ble_ll_set_advdata(ops, adv_data, sizeof(adv_data));
	ble_ll_set_scan_rsp(ops, scan_resp, sizeof(scan_resp));
	ble_ll_set_adv_interval(ops, 100);
	ble_ll_set_adv_en(ops, 1);
	ble_ll_open(ops, 0, 38);
	return wireless_pair ? RET_OK : RET_ERR;
}

int32 wireless_pair_deinit(struct bt_ops *ops)
{
	if (wireless_pair) {
		os_free(wireless_pair);
		ble_ll_close(ops);
	}
	return RET_OK;
}

#if 0
enum
{
    WIRELESS_WIFI,
    WIRELESS_BLE,
};

/*********************************************************************************************************************
 * 1、该demo主要实现蓝牙接收ssid和password,然后实现配网(需要将station配成有密码模式)
 * 2、适配的微信小程序是BLE配网
 * *******************************************************************************************************************/
struct ble_data
{
    uint8 type;
    uint8 size;
    //data
};

static uint8 pair_ack[2+6+4] = {
	// header
	0x02,
	0x0A,
	// adv addr	
	0x66,//0x11,
    0x55,//0x22,
    0x44,//0x33,
	0x33,//0x44,
	0x22,//0x55,
	0x11,//0x66,
	// adv data
	0x12,
	0x34,
	0xff,
	0xff,
};

static uint8 tx_gfsk[2+6+4] = {
	// header
	0x02,
	0x0A,
	// adv addr
	0xbc,//0x12,
	0x9a,//0x34,
	0x78,//0x56,
	0x56,//0x78,
	0x34,//0x9a,
	0x12,//0xbc,
	// adv data
	0x12,
	0x34,
	0x01,
	0xff,
};

static const uint8 pair_addr[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

struct app_ble_priv
{
    void *priv;
    void *ops;
    ble_recv cb;
};

static void *global_wireless_ops = NULL;
static struct app_ble_priv *g_user_priv = NULL;


uint8 crc8_payload(uint8 *p, uint8 len)
{
    uint8 crc;
    uint8 i;
    
    crc = 0;
    while(len--) {
        crc ^= *p++;
        for(i=0; i<8; i++) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            }else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void wireless_recv(void *priv,struct sk_buff *skb) 
{
    uint16_t pair_code;
    struct app_ble_priv *app_wireless = (struct app_ble_priv*)priv;
	printf("<");
    if((memcmp(skb->data+2, pair_addr, 6) == 0) && (crc8_payload(skb->data+2+6, 3) == skb->data[2+6+3])) 
    {
        
        pair_code = skb->data[2+6]|(skb->data[2+6+1]<<8);
		#if 1
        pair_ack[2+6] = pair_code & 0xff;
        pair_ack[2+6+1] = (pair_code>>8) & 0xff;
        pair_ack[2+6+2] = 0x01;
        pair_ack[2+6+3] = crc8_payload(pair_ack+2+6, 3);            
        os_sleep_ms(5);
        lmac_ble_tx(app_wireless->ops, pair_ack, sizeof(pair_ack));
		#endif
        //printf("%s:%d\tpair_code:%d\t%X\n",__FUNCTION__,__LINE__,pair_code,app_wireless->ops);
        //返回匹配码到应用层,应用层需要自己独立保存
        if(app_wireless->cb)
        {
            app_wireless->cb(app_wireless->priv,&pair_code,1);
        }
        //切换到wifi
        wireless_switch(WIRELESS_WIFI);

    }

}



static struct lmac_blenc_param data ={
    .recv_cb   = wireless_recv,

};
int start_wireless(void *ops,void *priv,uint8 chan,ble_recv b_recv)
{
    struct app_ble_priv *user_priv = NULL;
    if(g_user_priv)
    {
        return RET_ERR;
    }
    if(b_recv)
    {
        user_priv = (struct app_ble_priv *)malloc(sizeof(struct app_ble_priv ));
        user_priv->priv = priv;
        user_priv->ops = ops;
        user_priv->cb = b_recv;
    }
	data.priv = user_priv;
	data.chan = chan;
	lmac_bgn_ble_init(ops);
    lmac_bgn_module_switch_init(ops);
    int32 err = lib_blenc_init();
    if(err == RET_ERR)
    {
        if(user_priv)
        {
            free(user_priv);
        }
        return RET_ERR;
    }

    struct lmac_wifi_ble_switch_ctl switch_ctl = {0};
    switch_ctl.ble_priv.data = &data;
    switch_ctl.ble_priv.type = 0;
    switch_ctl.ap_ctl.switch_time = 32;
    switch_ctl.ap_ctl.period      = 1;
    switch_ctl.ap_ctl.arg         = NULL;
    switch_ctl.ap_ctl.hdl         = NULL;
    lmac_set_wifi_ble_switch_cfg(ops, &switch_ctl);
	

    g_user_priv = user_priv;
    global_wireless_ops = ops;
	extern struct sys_config sys_cfgs;
	
	if(!sys_cfgs.wireless_paircode)
	{
		wireless_switch(WIRELESS_BLE);
	}
	return RET_OK;
    
}


//测试用例,用户根据这个参考
int8 wireless_recv_data(void *priv,uint8 *data,int len)
{
    uint16_t *paircode = (uint16_t*)data;
	//配对码保存
	extern struct sys_config sys_cfgs;
	sys_cfgs.wireless_paircode = paircode[0];
	syscfg_save();
    return RET_OK;
}



void wireless_switch(uint8_t type)
{
    static uint8_t type_now = -1;
    if(global_wireless_ops)
    {

        if(type_now != type)
        {
            type_now = type;
            if(type == WIRELESS_BLE)
            {
                lmac_wifi_switch_ble(global_wireless_ops);
            }
            else if(type == WIRELESS_WIFI)
            {
                lmac_ble_switch_wifi(global_wireless_ops);
            }
        }
    }
}



void wireless_send(uint16_t pair_code)
{
    if(global_wireless_ops)
    {
        wireless_switch(WIRELESS_BLE);
        tx_gfsk[2+6] = pair_code & 0xff;
        tx_gfsk[2+6+1] = (pair_code>>8) & 0xff;
        tx_gfsk[2+6+2] = 0x02;
        tx_gfsk[2+6+3] = crc8_payload(tx_gfsk+2+6, 3);
        lmac_ble_tx(global_wireless_ops, tx_gfsk, sizeof(tx_gfsk));
        wireless_switch(WIRELESS_WIFI);
    }
}
#endif