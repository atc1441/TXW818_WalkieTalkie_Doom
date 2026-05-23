#ifndef __NAND_OPS_H__
#define __NAND_OPS_H__

#include "typesdef.h"
#include "osal/mutex.h"
#include "lib/spi_nand/nand_chip.h"


#define NAND_MAX_CHIPS 5


#ifndef BUS_FREQ
	#define BUS_FREQ 8*1000*1000
#endif

#ifndef BUS_WIREMODE
	#define BUS_WIREMODE SPI_WIRE_NORMAL_MODE
#endif

#ifndef BUS_CLOCKMODE
	#define BUS_CLOCKMODE SPI_CPOL_1_CPHA_1
#endif


enum nand_ioctl_cmd
{
    NAND_RD_CMD_SET,
    NAND_WR_CMD_SET,
    NAND_WIREMODE_SET,
    NAND_RD_WIREMODE_SET,
    NAND_WR_WIREMODE_SET,
    NAND_FREQ_SET,
};


struct nand_bbt_descr {
	int page;
	int col;
	int len;
	int maxblocks;
    uint8_t header[4];
};

struct nand_link_table {
    uint32_t header;
    uint16_t lba[16];
    uint16_t pba[16];
    uint16_t cnt;
    uint16_t size;
};

struct spi_nand {
    struct dev_obj dev;
    uint8_t *bbt;
    uint32_t col;
    struct nand_chip *chip;
    struct os_mutex mutex;
    int (*page_read)(struct spi_nand *nand, uint32_t ofs, uint8* buf, uint32_t len);
    int (*page_write)(struct spi_nand *nand, uint32_t ofs, uint8* buf, uint32_t len);
    void (*erase)(struct spi_nand *nand, uint32_t block);
    void (*ioctl)(struct spi_nand *nand);
};

int nand_init(struct spi_nand *nand, struct nand_chip *chip);
int nand_deinit(struct spi_nand *nand);
int nand_read(struct spi_nand *nand, uint32_t ofs, uint8_t *buf, uint32_t len);
int nand_write(struct spi_nand *nand, uint32_t ofs, uint8_t *buf, uint32_t len);
int nand_erase(struct spi_nand *nand, uint32_t block);
int nand_ioctl(struct spi_nand *nand, uint32_t cmd, uint32_t para1, uint32_t para2);

uint8_t bbt_is_bad(struct spi_nand *nand, int block);
void bbt_mark_entry(struct spi_nand *nand, int block);
#endif
