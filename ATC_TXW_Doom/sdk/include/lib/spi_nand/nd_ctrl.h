#ifndef __ND_CTRL_H__
#define __ND_CTRL_H__

#include "typesdef.h"
#include "list.h"
#include "errno.h"
#include "dev.h"
#include "devid.h"
#include "hal/spi.h"
#include "lib/spi_nand/nand_chip.h"
#include "osal/mutex.h"

#define NAND_PRINT_EN 1
#if (NAND_PRINT_EN)
	#define NAND_PRINT(fmt, args...)                     printf(fmt, ##args)
#else
	#define NAND_PRINT(...)
#endif

enum line_mode{
    OP_ADR_DAT_111,
    OP_ADR_DAT_112,
    OP_ADR_DAT_114,
    OP_ADR_DAT_118,
    OP_ADR_DAT_122,
    OP_ADR_DAT_144,
    OP_ADR_DAT_188,
    OP_ADR_DAT_222,
    OP_ADR_DAT_444,
    OP_ADR_DAT_888,
};

struct spic {
    struct spi_device *spi;
    unsigned int spi_devid;
    unsigned int freq;
    unsigned int work_mode;
    unsigned int wire_mode;
    unsigned int clk_mode;
    unsigned int ntarget;
    unsigned int cur_cs;
};

struct nd_ctrl {
	struct nand_chip *chip;
    struct spic spic;
    int (*trans)(struct nd_ctrl *ctrl, uint32 cmd, uint32 addr, uint32 addr_len, uint32 dummys, uint8_t *buf, uint32 len);
    int (*recv)(struct nd_ctrl *ctrl, uint32 cmd, uint32 addr, uint32 addr_len, uint32 dummys, uint8_t *buf, uint32 len);
    int (*transx)(struct nd_ctrl *ctrl, uint32 cmd, uint32 addr, uint32 addr_len, uint32 dummys, uint8_t *buf, uint32 len);
    int (*recvx)(struct nd_ctrl *ctrl, uint32 cmd, uint32 addr, uint32 addr_len, uint32 dummys, uint8_t *buf, uint32 len);
    int (*target)(struct nd_ctrl *ctrl, uint32 cs);
};

struct nd_ctrl* spic_ctrl_init(struct spic *spic);
int nd_ctrl_init(struct nd_ctrl *nd);

#endif
