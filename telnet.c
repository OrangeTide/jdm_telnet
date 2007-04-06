/* PUBLIC DOMAIN - March 20, 2007 - Jon Mayo */
/* TODO:
 * + handle TCP Urgent/OOB state changes (like SYNCH)
 */
#define NDEBUG
/* References:
 * RFC 854 - Telnet Protocol Specification
 * RFC 855 - Telnet Option Specification
 * RFC 857 - Telnet Echo Option
 * RFC 859 - Telnet Status Option
 * RFC 861 - Telnet Extended Options: List Options
 * RFC 1143 - The Q Method of Implementing TELNET Option Negotiation
 * RFC 1184 - Telnet Linemode Option
 * RFC 1572 - Telnet Environment Option
 *
 * others?
 * RFC 1080
 * RFC 2066
 * RFC 1647
 *
 * special characters to consider
 * 255 IAC
 *
 * commands that don't accept options
 * 244 IP
 * 245 AO
 * 246 AYT
 * 247 EC
 * 248 EL
 * 249 GA
 *
 * requests:
 * DO ----- WILL .: initiator begins using option
 *     \
 *      --- WONT .: responder must not use option
 *
 * WILL --- DO   .: responder begins using after sending DO
 *       \
 *        - DONT .: initiator must not use option
 *
 * DONT --- WONT .: notification of initator deactivated option
 *
 * WONT --- DONT .: notification that responder should deactive option
 *
 * (avoid loops of DONT-WONT)
 *
 * more info:
 * http://www.ics.uci.edu/~rohit/IEEE-L7-v2.html
 * http://www.garlic.com/~lynn/rfcietff.htm
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TELCMDS
#define TELOPTS

#include <arpa/telnet.h>
#include "telnet.h"
#include "macros.h"

enum telnet_event_type {
    TelnetEventText,
    TelnetEventControl
};

enum telnet_state {
    TelnetStateText,
    TelnetStateIacIac,          /* IAC escape */
    TelnetStateIacCommand,      /* WILL WONT DO DONT ... */
    TelnetStateIacOption,       /* TELOPT_xxx */
    TelnetStateSb,              /* SB ... */
    TelnetStateSbIac            /* IAC inside Sb */
};

struct telnet_event {
    enum telnet_event_type type;
};

struct telnet_info {
    enum telnet_state telnet_state;
    const char *inbuf;
    size_t inbuf_len, inbuf_current;
    unsigned char command, option;
    size_t extra_len;
    size_t extra_max;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
    char extra[];
#else
    char extra[0];
#endif
};

struct telnet_info *telnet_create(size_t extra_max) {
    struct telnet_info *ret;
    /* if extra_max is 0 pick a reasonable size */
    if(!extra_max) extra_max=16;
    ret=malloc(sizeof *ret + extra_max);
    ret->telnet_state=TelnetStateText;
    ret->extra_len=0;
    ret->extra_max=extra_max;
    ret->inbuf_len=0;
    ret->inbuf_current=0;
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

#ifndef NDEBUG
static void hexdump(int n, const unsigned char *d) {
    int i;
    for(i=0;i<n;i++) fprintf(stderr, " %02X", d[i]);
}
#endif

/* get the next block of regular text from the telnet engine
 * for IAC IAC the call will be broken up into two parts.
 * this is because the input buffer is not modified.
 */
int telnet_gettext(struct telnet_info *ts, size_t *len, const char **ptr) {
    size_t current, newlen;

    assert(ts != NULL);
    assert(ptr != NULL);
    assert(len != NULL);

    if(ts->inbuf_current>=ts->inbuf_len) {
        /* no more data */
        return 0;
    }

    switch(ts->telnet_state) {
        case TelnetStateIacIac:
        case TelnetStateText:
            *ptr=ts->inbuf+ts->inbuf_current;
            current=ts->inbuf_current;
            newlen=0;
            if(ts->telnet_state==TelnetStateIacIac) {
                ts->telnet_state=TelnetStateText;
                /* skip the IAC in the loop below */
                current++;
                newlen++;
            }
            for(;current<ts->inbuf_len;current++,newlen++) {
                    if((unsigned char)ts->inbuf[current]==IAC) {
                        /* TODO: handle IAC IAC here as well */
                        ts->telnet_state=TelnetStateIacCommand;
#ifndef NDEBUG
                        fprintf(stderr, "command: ");
                        hexdump(8, &ts->inbuf[current]);
                        fprintf(stderr, "\n");
#endif
                        current++;
                        break;
                    }
            }
#ifndef NDEBUG
            fprintf(stderr, "curr: %d %d len: %d\n", current, ts->inbuf_current, ts->inbuf_len);
#endif
            *len=newlen;
            assert(*len >= 0);
            ts->inbuf_current=current;
            return 1;
        case TelnetStateIacCommand:
        case TelnetStateIacOption:
        case TelnetStateSb:
        case TelnetStateSbIac:
            return 0;
    }
    fprintf(stderr, "Invalid telnet state %d in %p\n", ts->telnet_state, (void*)ts);
    abort();
}

/* get the next control item from the telnet engine.
 * ts - telnet info/state
 * command - pointer to a single unsigned char
 * option - pointerto a single unsigned char
 * extra_len - pointer to write the length of the extra data
 * extra - extra data buffer (for SB)
 */
int telnet_getcontrol(struct telnet_info *ts, unsigned char *command, unsigned char *option, size_t *extra_len, const char **extra) {
    unsigned char tmp;
again:
    if(ts->inbuf_current>=ts->inbuf_len) {
        /* no more data */
        return 0;
    }

    switch(ts->telnet_state) {
        case TelnetStateIacCommand:
            /* look for command parameter to IAC */
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current];
#ifndef NDEBUG
            fprintf(stderr, "IAC %s\n", TELCMD(tmp));
#endif
            switch(tmp) {
                case IAC:
                    ts->telnet_state=TelnetStateIacIac;
                    return 0;
                case DONT:
                case DO:
                case WONT:
                case WILL:
                    /* the following is for 3-byte IAC <cmd> <opt> codes */
                    ts->telnet_state=TelnetStateIacOption;
                    ts->command=tmp;
                    ts->inbuf_current++;
                    goto again;
                case EOR: /* End of Record - RFC 885 */
                    /* This only shows up if TELOPT_EOR was negotiated */
                case ABORT: /* Abort - RFC 1184 */
                    /* This only shows up if TELOPT_LINEMODE was negotiated */
                    /* should be treated the same as IAC IP in most cases */
                case SUSP: /* Suspend - RFC 1184 */
                    /* This only shows up if TELOPT_LINEMODE was negotiated */
                case xEOF: /* End of File - RFC 1184 */
                    /* This only shows up if TELOPT_LINEMODE was negotiated */
                    /* user sent an EOF "character"/signal */
                case EC: /* Erase Character */
                case EL: /* Erase Line */
                case GA: /* Go Ahead */
                case AYT: /* Are You There */
                case AO: /* Abort Output */
                case IP: /* Interrupt Process */
                case NOP: /* No Operations */
                case BREAK: /* special key with a vague definition - RFC 854 */
                    /* the following is for 2-byte IAC <cmd> codes */
                    ts->telnet_state=TelnetStateIacOption;
                    ts->command=tmp;
                    ts->inbuf_current++;
                    ts->telnet_state=TelnetStateText;
                    ts->option=0;
                    if(command) *command=ts->command;
                    if(option) *option=0;
                    if(extra_len) *extra_len=0;
                    if(extra) *extra=0;
                    return 1;
                case DM: /* data mark */
                    /* marks the location in the stream a high priority
                     * telnet urgant OOB message was sent. 
                     * normally all unprocessed data is discarded from the
                     * point of the urgant message to the DM. 
                     * if an IAC DM is received but no urgant messages are
                     * pending then the DM is ignored (treated as an IAC NOP)
                     */
                    /* the following is for 2-byte IAC <cmd> codes */
                    ts->telnet_state=TelnetStateIacOption;
                    ts->command=tmp;
                    ts->inbuf_current++;
                    ts->telnet_state=TelnetStateText;
                    ts->option=0;
                    if(command) *command=ts->command;
                    if(option) *option=0;
                    if(extra_len) *extra_len=0;
                    if(extra) *extra=0;
                    return 1;
                case SB: /* Subnegotiation Begin */
                    /* format: IAC SB <option> ... IAC SE */
                    /* TODO: handle this */
                    ts->command=SB;
                    ts->telnet_state=TelnetStateSb;
                    ts->extra_len=0;
                    ts->inbuf_current++;
                    goto again;
                case SE: /* Subnegotiation End */ 
                    /* TODO: this is an error in this state. 
                     * we sohuld ignore it */
                    ts->inbuf_current++;
                    return 0;
            }
            /* TODO: do something with unknown codes */
#ifndef NDEBUG
            fprintf(stderr, "unknown code %u\n", tmp);
#endif
            abort(); /* TODO: maybe treat this as 2 byte code? */
             
        case TelnetStateIacOption:
            /* the following is for 3-byte IAC <cmd> <opt> codes */
            ts->telnet_state=TelnetStateText;
            ts->option=ts->inbuf[ts->inbuf_current++];
            if(command) *command=ts->command;
            if(option) *option=ts->option;
            if(extra_len) *extra_len=0;
            if(extra) *extra=0;
            return 1;
        case TelnetStateSb:
            /* TODO: look for IAC */
            /* TODO: push data into buffer */
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current++];
            if(tmp==IAC) {
                ts->telnet_state=TelnetStateSbIac;
            } else {
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=tmp;
                }
            }
            goto again;
        case TelnetStateSbIac:
            /* TODO: look for IAC SE */
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current++];
            if(tmp==IAC) {
                /* IAC IAC escape */
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=tmp;
                }
            } else if(tmp==SE) {
                /* IAC SE */
                ts->telnet_state=TelnetStateText;
                if(command) *command=ts->command;
                if(option) *option=ts->extra[0];
                if(extra_len) *extra_len=ts->extra_len;
                if(extra) *extra=ts->extra;
                return 1;
            } else {
                /* something unknown. don't escape anything */
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=(char)IAC;
                }
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=tmp;
                }
            }
            goto again;
        case TelnetStateIacIac:
        case TelnetStateText:
            /* nothing to get */
            return 0;
    }
#ifndef NDEBUG
    fprintf(stderr, "fell through state %u\n", ts->telnet_state);
#endif
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
    /* TODO: check that the buffer was completely consumed */

    ts->inbuf=0;
    ts->inbuf_len=0;
    return 1;
}

/* releases the telnet state */
void telnet_free(struct telnet_info *ts) {
}

/**************************** TEST & EXAMPLE CODE ****************************/

/* USAGE:
 *  + TODO - explain how to use it
 * IDEAS:
 *  + combine telnet_end() and telnet_continue() ?
 * TODO:
 *   + handle control messages and responses in the struct
 *   + make the API very light weight
 *   + add ways to automate the building of control messages
 */

int main() {
    const struct {
        int n;
        char *b;
    } test_data[] = {
        { 6, "hello " },
        { 6, "world\n" },
        { 9, "\33[0;1;32m" }, /* ESC [ 0 m  */
        { 2, "\377\377" }, /* IAC IAC */
        { 3, "\377\373\1" }, /* IAC WILL TELOPT_ECHO */
        { 2, "\377\366" }, /* IAC AYT */
        { 3, "\377\361\377" }, /* IAC NOP IAC */
        { 2, "\376\42" }, /* DONT TELOPT_LINEMODE */
        { 15, "this is a test\n" },
        { 2, "\377\377" }, /* IAC IAC */
        { 4, "\33[0m" }, /* ESC [ 0 m  */
        { 2, "\377\377" }, /* IAC IAC */
        { 8, "\377\372\42\1\1\377\360" }, /* IAC SB TELOPT_LINEMODE MODE EDIT IAC SE */
    };
    int i;
    struct telnet_info *ts;

    ts=telnet_create(0);
    for(i=0;i<NR(test_data);i++) {
        telnet_begin(ts, test_data[i].n, test_data[i].b);
        while(telnet_continue(ts)) {
            const char *text_ptr;
            size_t text_len;
            const char *ex;
            size_t exlen;
            unsigned char cmd, opt;
            /* TODO: call telnet_getXXX in a loop until 0 */
            /* handle regular data */
            if(telnet_gettext(ts, &text_len, &text_ptr)) {
                /* Dump all normal text to stdout */
#ifndef NDEBUG
                fprintf(stderr, "text_len=%d\n", text_len);
#endif
                if(fwrite(text_ptr, 1, text_len, stdout)!=text_len) {
                    perror("fwrite()");
                }
#ifndef NDEBUG
                fputc('(', stdout);
                fputc(')', stdout);
#endif
            }

            /* handle control data */
            if(telnet_getcontrol(ts, &cmd, &opt, &exlen, &ex)) {
                /* log control messages to stderr */
                fprintf(stderr, "\nControl message: IAC");
                if(TELCMD_OK(cmd)) {
                    fprintf(stderr, " %s", TELCMD(cmd));
                } else {
                    fprintf(stderr, " %u", cmd);
                }
                if(opt) {
                    if(TELOPT_OK(opt)) {
                        fprintf(stderr, " %s", TELOPT(opt));
                    } else {
                        fprintf(stderr, " %u", opt);
                    }
                }
                if(ex && exlen>0) {
#ifndef NDEBUG
                        hexdump(exlen, ex);
#endif
                }
                fprintf(stderr, "\n");

            }
        }
        telnet_end(ts);
    }
    telnet_free(ts);
    fputc('\n', stdout);
    return 0;
}
