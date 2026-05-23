#ifndef __IMAGE_GENERATION_DEMO_H
#define __IMAGE_GENERATION_DEMO_H

#include "llm/llm_api.h"

#include "syscfg.h"
#include "devid.h"
#include "keyWork.h"
#include "keyScan.h"
#include "mp3_decode.h"
#include "audio_dac.h"
#include "stream_define.h"
#include "stream_frame.h"
#include "utlist.h"
#include "jpgdef.h"
#include "lwip/netdb.h"

enum ig_session_type {
    IG_SESSION_UNKNOWN  = 0,
    IG_SESSION_STT      = 1,
    IG_SESSION_TTI      = 2
};

struct ig_event_msg {
    uint16  type;
    uint16  event;
    char    *msg;
    uint32  msg_len;
};

struct image_generation_manage {
    void    *stt_session;
    void    *tti_session;    
    stream  *mic_stream;
    stream  *screen_stream;
    uint32  key_triggered:1,        // 按键触发标志位
            voice_triggered:1,      // 语音触发标志位
            auadc_model_start: 1,   // 音频采集
            rev: 29;
    uint64  time_stamp;             // 时间戳
    RBUFFER_DEF_R(event_queue, struct ig_event_msg);    
    struct rbuffer sound_rb;
};

#endif  //__IMAGE_GENERATION_DEMO_H

