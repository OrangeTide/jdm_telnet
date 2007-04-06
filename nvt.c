/* nvt.c - network virtual terminal - abstraction for telnet protocol */
/* March 29, 2007 */
/* Purpose:
 * + track state 
 * + implement Q algorithm
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdbool.h>
#else
enum bool { false, true };
#define bool enum bool
#endif
struct nvt_info {
    unsigned columns, lines;
    bool echo;
    bool linemode;
};
