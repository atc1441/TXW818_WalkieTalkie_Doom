#ifndef __NAND_CHIP_H__
#define __NAND_CHIP_H__


#include "typesdef.h"
#include "lib/spi_nand/nd_ctrl.h"



#define DEV_REG_CNT (4)

#define NAND_MAX_BLOCKS		32768	/**< Max number of Blocks */
#define NAND_MAX_PAGE_SIZE	2048	/**< Max page size of NAND flash */
#define NAND_MAX_OOB_SIZE	64	    /**< Max OOB bytes of a NAND flash page */
#define NAND_ECC_SIZE		256	    /**< ECC block size */
#define NAND_ECC_BYTES		6	    /**< ECC bytes per ECC block */


enum nf_op_err{
    nf_ECC_ERR = 1+0x10,
    nf_BBLOCK_ERR = 2+0x10,
	nf_FAIL = 3+0x10,
    nf_OOR  = 4+0x10,
};

struct nf_cmd_set{
    uint8_t wel;
    uint8_t wdl;
    uint8_t get_st;
    uint8_t set_st;
    uint8_t page_read;
    uint8_t page_prog;
    uint8_t erase;
    uint8_t reset;
};

/*
 * This enum contains ECC Mode
 */
typedef enum {
	NAND_ECC_NONE,
	NAND_ECC_SOFT,          //不支持
	NAND_ECC_HW,			//不支持
	NAND_ECC_HW_SYNDROME,	//不支持
	NAND_ECC_HW_OOB_FIRST,	//不支持
	NAND_ECC_SOFT_BCH,
	NAND_ECC_ONDIE	/**< On-Die ECC */
} Nand_EccMode;

struct NandOobFree {
	uint32_t Offset;
	uint32_t Length;
};

/*
 * ECC layout control structure. Exported to user space for
 * diagnosis and to allow creation of raw images
 */
struct NandEccLayout {
	uint32_t EccBytes;
	uint32_t EccPos;
	uint32_t OobAvail;
	struct NandOobFree OobFree;
};

struct NandBuffers {
	uint8_t DataBuf[NAND_MAX_PAGE_SIZE];
	uint8_t	EccCode[NAND_MAX_OOB_SIZE]; /**< Buffer for stored ECC */    
	uint8_t	EccCalc[NAND_MAX_OOB_SIZE];	/**< Buffer for calculated ECC */
	int32_t pcache;
};

struct NandEccStatus {
	uint32_t Corrected;
	uint32_t Failed;
	uint32_t BadBlocks;
	uint32_t BbtBlocks;
};

struct Ops {
    uint16_t col;
    uint16_t len;
    uint8_t *buf;
};

struct NandEccCtrl {
	Nand_EccMode Mode;
	uint32_t EccSteps;		/**< Number of ECC steps for the flash page */
	uint32_t EccSize;		/**< ECC size */
	uint32_t EccBytes;	    /**< Number of ECC bytes for a block */
	uint32_t EccTotalBytes;	/**< Total number of ECC bytes for Page */
	uint32_t Strength;
	struct NandEccLayout *Layout;	
	void *Priv;
};

struct nand_chip {
	uint32_t header;
    uint32_t \
		rd_opc :8,
		rd_lc  :8,
		wr_opc :8,
		wr_lc  :8;
		
    uint16_t rd_lmode: 8,
			 wr_lmode: 8;

	uint32_t dev_size;

	uint32_t block_per_chip: 16,
			 oob_size      : 15,
	         odd_block_en  : 1;
/*
    page : 2^11
    block: 2^17
*/
	uint32_t \
		ofs_shift_block : 8,
		ofs_shift_page  : 8,
		page_shift_block: 8,
		reg_cnt         : 8;
	
    uint8_t  reg_adr[DEV_REG_CNT];
    uint8_t  reg_ini[DEV_REG_CNT];
    uint32_t oip_us;

	struct nf_cmd_set ct;
	struct NandEccCtrl EccCtrl;		          /**< ECC configuration parameters */
    struct NandBuffers *Buffers;
    struct nd_ctrl *ctrl;
};

int nf_init(struct nand_chip *chip);
uint8_t nf_get_feature(struct nand_chip *chip, uint8_t sr_adr, uint8_t mask);
int nf_set_feature(struct nand_chip *chip, uint8_t sr_adr, uint8 mask, uint8_t val);
int nf_set_ftbit(struct nand_chip *chip, uint32_t ftb, uint32_t en);
int nf_read_page(struct nand_chip *chip, uint32_t page, uint32_t paddr, uint8* buf, uint32_t len);
int nf_erase(struct nand_chip *chip, uint32_t block);
int nf_write_page(struct nand_chip *chip, uint32_t page, uint32_t paddr, uint8* buf, uint32_t len);
int nf_update_page(struct nand_chip *chip, uint32_t page, uint32_t paddr, uint32_t buf, uint32_t len);
int nf_isbad_block(struct nand_chip *chip, uint32_t block);
int nf_bad_mark(struct nand_chip *chip, uint32_t block);
uint8_t nf_get_eccs(struct nand_chip *chip);

int nf_is_ecc_error(struct nand_chip *chip);
int nf_is_erase_fail(struct nand_chip *chip);
int nf_is_prog_fail(struct nand_chip *chip);

#endif
