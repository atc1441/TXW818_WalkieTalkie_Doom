#include "sys_config.h"
#include "typesdef.h"
#include "dev.h"
#include "devid.h"
#include "hal/gpio.h"
#include "hal/adc.h"
#include "hal/spi_nor.h"
#include "buttons.h"
#include <stdio.h>

#define BUTTONS_ADC_PIN  PA_3

static int hw_4mb = 0;

void buttons_hw_detect(void)
{
    struct spi_nor_flash *flash = (struct spi_nor_flash *)dev_get(HG_FLASH0_DEVID);

    printf("Flash infos: \r\n");
    if (flash) {
        printf("  size      : %u bytes (%u MB)\r\n", flash->size, flash->size / (1024 * 1024));
        printf("  vendor_id : 0x%02X\r\n", flash->vendor_id);
        printf("  product_id: 0x%04X\r\n", flash->product_id);
        printf("  sector_sz : %u bytes\r\n", flash->sector_size);
        printf("  page_sz   : %u bytes\r\n", flash->page_size);
        if (flash->size >= (4 * 1024 * 1024)) {
            hw_4mb = 1;
            printf("  hw variant: 4MB (gelasert)\r\n");
        } else {
            printf("  hw variant: 2MB\r\n");
        }
    } else {
        printf("  ERROR: flash device not found!\r\n");
    }
}

void buttons_init(void)
{
    struct adc_device *adc = (struct adc_device *)dev_get(HG_ADC0_DEVID);

    /* 100K-Pulldown, kein interner Pull */
    gpio_set_mode(BUTTONS_ADC_PIN, GPIO_PULL_NONE, GPIO_PULL_LEVEL_100K);
    adc_open(adc);
    adc_add_channel(adc, BUTTONS_ADC_PIN);
}

btn_id_t buttons_read(void)
{
    struct adc_device *adc = (struct adc_device *)dev_get(HG_ADC0_DEVID);
    uint32 val = 0;

    adc_get_value(adc, BUTTONS_ADC_PIN, &val);

    if (hw_4mb) {
        /* 4MB-Hardware (gelasert) */
        if (val < 500)  return BTN_UP;
        if (val < 1500) return BTN_PRESS;
        if (val < 2500) return BTN_DOWN;
    } else {
        /* 2MB-Hardware */
        if (val < 1500) return BTN_UP;
        if (val < 2500) return BTN_DOWN;
        if (val < 3850) return BTN_PRESS;
    }
    return BTN_NONE;
}

uint32_t buttons_raw(void)
{
    struct adc_device *adc = (struct adc_device *)dev_get(HG_ADC0_DEVID);
    uint32 val = 0;

    adc_get_value(adc, BUTTONS_ADC_PIN, &val);

    return val;
}
