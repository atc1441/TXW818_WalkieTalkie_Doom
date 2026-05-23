#ifndef __AUDIO_DAC_H
#define __AUDIO_DAC_H
void audio_da_init();
void audio_dac_set_filter_type(int filter_type);
int get_audio_dac_set_filter_type(void);
int audio_dac_get_samplingrate(void);
void audio_da_recfg(uint32_t hz);
void audac_enable_play(void);
void audac_disable_play(void);
uint8_t audac_get_play_state(void);
uint8_t audac_wait_empty(void);
#endif