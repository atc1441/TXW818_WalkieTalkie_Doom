#include "sys_config.h"
#include "typesdef.h"
#include "lib/lmac/lmac.h"
#include "lib/lmac/lmac_def.h"
#include "lib/ble/ble_demo.h"
#include "lib/ble/ble_def.h"

extern int32 wireless_pair_tx(struct bt_ops *ops, uint8 *data, uint32 len);
extern int32 wireless_pair_init(struct bt_ops *ops);
extern int32 wireless_pair_deinit(struct bt_ops *ops);

const uint8 tx_payload_gfsk[2+6+31] = {
	// header
	0x02,
	0x25,
	// adv addr
	0x6d,
	0xef,
	0xa5,
	0x72,
	0x39,
	0xd0,
	// adv data
	0x02,
	0x01,
	0x06,
	0x03,
	0x02,
	0xf0,
	0xff,
	0x08,
	0xff,
	0x01,
	0x02,
	0x03,
	0x04,
	0x05,
	0x06,
	0x07,
	0x08,
	0x09,
	0x0a,
	0x0b,
	0x0c,
	0x0d,
	0x0e,
	0x0f,
	0x10,
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,


};
extern void *g_ops;

void ble_reset_pair_network(uint8 chan, uint8 mode)
{
	struct lmac_ops *lops = (struct lmac_ops *)g_ops;
	struct bt_ops *bt_ops = (struct bt_ops *)lops->btops;

	if (!g_ops) {
		os_printf("%s:%d err\n",__FUNCTION__,__LINE__);
		return;
	}

	//进入蓝牙配网
    if (mode)
    {
		if (ble_demo_start(bt_ops, mode - 1)) {
			return;
		} else {
			os_printf("\n\nset ble mode = %d \r\n\n", mode);
		}
    }
    //发送小无线信号
    else
    {
        wireless_pair_init(bt_ops);
        wireless_pair_tx(bt_ops, (uint8 *)tx_payload_gfsk, sizeof(tx_payload_gfsk));
		wireless_pair_deinit(bt_ops);
    }
}

