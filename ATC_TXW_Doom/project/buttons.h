#ifndef _BUTTONS_H_
#define _BUTTONS_H_

typedef enum {
    BTN_NONE  = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_PRESS,
} btn_id_t;

void    buttons_hw_detect(void); /* Flash-Groesse lesen + hw_4mb setzen — immer aufrufen */
void    buttons_init(void);
btn_id_t buttons_read(void);   /* Non-blocking, aktueller Zustand */
uint32_t buttons_raw(void);

#endif /* _BUTTONS_H_ */
