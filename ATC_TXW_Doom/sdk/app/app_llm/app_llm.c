#include "basic_include.h"

int sys_app_llm_init(void)
{
#if LLM_TEST == 1
extern void llm_stt_test(void);
    llm_stt_test();
#elif LLM_TEST == 2
extern void llm_tts_test(void);
    llm_tts_test();
#elif LLM_TEST == 3
extern void llm_tti_test(void);
    llm_tti_test();
#elif LLM_TEST == 4
extern void llm_chat_test(void);
    llm_chat_test();
#endif

#ifdef IMAGE_GENERATION_DEMO
extern void image_generation_demo(void);
    image_generation_demo();
#endif

#ifdef COZE_DEMO
extern void coze_demo(void);
    coze_demo();
#endif
    return 0;
}

