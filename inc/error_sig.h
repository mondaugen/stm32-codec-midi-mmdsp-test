#ifndef ERROR_SIG_H
#define ERROR_SIG_H 

/* Routines that signal errors by turning on LEDs or whatever */

#include "leds.h" 

/* Initialize hardware needed to indicate errors */
static inline void error_sig_init(void)
{
    LEDs_init();
}

static inline void error_sig_started_waiting(void)
{
    LEDs_redSet();
}

static inline void error_sig_done_waiting(void)
{
    LEDs_redReset();
}


#endif /* ERROR_SIG_H */
