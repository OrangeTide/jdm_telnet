/* PUBLIC DOMAIN - March 20, 2007 - Jon Mayo */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/telnet.h>
#include "telnet.h"
#include "macros.h"

enum telnet_event_type {
    TelnetEventText,
    TelnetEventControl
};

enum telnet_state {
    TelnetStateText,
    TelnetStateIacStart,
    TelnetStateIacCommand,    /* WILL WONT DO DONT */
    TelnetStateIacOption,
};

struct telnet_event {
    enum telnet_event_type type;
};

struct telnet_info {
    enum telnet_state telnet_state;
    const char *inbuf;
    size_t inbuf_len, inbuf_current;
    unsigned char command, option;

};

struct telnet_info *telnet_create(void) {
    struct telnet_info *ret;
    ret=malloc(sizeof *ret);
    ret->telnet_state=TelnetStateText;
    return ret;
}

/* loads a buffer to the telnet engine */
int telnet_begin(struct telnet_info *ts, size_t inbuf_len, const char *inbuf) {
    assert(ts != NULL);
    assert(ts->inbuf == NULL);
    ts->inbuf=inbuf;
    ts->inbuf_len=inbuf_len;
    ts->inbuf_current=0;
    return 1;
}

/* get the next block of regular text from the telnet engine 
 * for IAC IAC the call will be broken up into two parts.
 * this is because the input buffer is not modified.
 */
int telnet_gettext(struct telnet_info *ts, size_t *len, const char **ptr) {
    assert(ts != NULL);
    assert(ptr != NULL);
    assert(len != NULL);
    if(ts->telnet_state==TelnetStateText) {
        size_t tmplen;
        *ptr=ts->inbuf+ts->inbuf_current;
        for(tmplen=ts->inbuf_current;tmplen<ts->inbuf_len;tmplen++) {
                if((unsigned char)ts->inbuf[tmplen]==IAC) {
                    ts->telnet_state=TelnetStateIacStart;
                    break;
                }
        } 
        *len=tmplen-ts->inbuf_current;
        ts->inbuf_current=tmplen;
        return 1;
    }
    return 0;
}

/* get the next control item from the telnet engine */
int telnet_getcontrol(struct telnet_info *ts) {
again:
    if(ts->inbuf_current>=ts->inbuf_len) {
        /* no more data */
        return 0;
    }
    switch(ts->telnet_state) {
        case TelnetStateIacStart:
            ts->telnet_state=TelnetStateIacCommand;
            /* TODO: look for IAC IAC */
            ts->command=ts->inbuf[ts->inbuf_current++];
            goto again;
        case TelnetStateIacCommand:
            ts->telnet_state=TelnetStateIacOption;
            ts->command=ts->inbuf[ts->inbuf_current++];
            goto again;
        case TelnetStateIacOption:
            ts->telnet_state=TelnetStateText;
            /* TODO: return command and option */
            return 1;
            goto again;
        case TelnetStateText:
            return 0;
    }
    abort();
}

/* returns true while telnet_getXXX() can still be called */
int telnet_continue(struct telnet_info *ts) {
    return ts->inbuf_current < ts->inbuf_len;
}

/* finishes an update cycle. the buffer passed as telnet_begin is no longer
 * referenced after this function.
 */
int telnet_end(struct telnet_info *ts) {
    ts->inbuf=0;
    ts->inbuf_len=0;
    return 1;
}

/* releases the telnet state */
void telnet_free(struct telnet_info *ts) {
}

/* USAGE:
 * size_t text_len;
 * char *text_ptr;
 * ts=telnet_create();
 * while(1) {
 *   len=read(fd, buf);
 *   telnet_begin(ts, len, buf);
 *   while(telnet_continue(ts)) {
 *     if(telnet_gettext(ts, &text_len, &text_ptr)) {
 *       process_text(text_len, text_ptr);
 *     }
 *     if(telnet_getcontrol(ts)) {
 *       respond_control(control_len, control_ptr);
 *     }
 *   }
 *   telnet_end(ts);
 * }
 *
 * IDEAS: combine telnet_end() and telnet_continue() ?
 * TODO: 
 *   + handle control messages and responses in the struct
 *   + make the API very light weight
 *   + add ways to automate the building of control messages
 */

int main() {
    struct telnet_info *ts;
    ts=telnet_create();
    telnet_free(ts);
    return 0;
}

