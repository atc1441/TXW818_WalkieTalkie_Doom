#ifndef _SOUNDS_H_
#define _SOUNDS_H_

#include "sys_config.h"

#ifdef IMAGE_GENERATION_DEMO
// 声明数组
extern const char beep[];
extern unsigned int beep_size;

extern const char yilianjie[];
extern unsigned int yilianjie_size;

extern const char yiduankai[];
extern unsigned int yiduankai_size;

extern const char fasong[];
unsigned int fasong_size;

// 如果还有其他数组，也可以在这里声明
// extern const unsigned char another_sound[];
// extern const unsigned int another_sound_size;

#endif // IMAGE_GENERATION_DEMO
#endif // _SOUNDS_H_

