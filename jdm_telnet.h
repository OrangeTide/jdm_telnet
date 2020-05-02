/* PUBLIC DOMAIN - March 20, 2007 - Jon Mayo
 * Last Update: May 1, 2020
 */
/* USAGE:
 *
 * Define JDM_TELNET_IMPLEMENTATION in exactly one source file.
 * Optionally define JDM_TELNET_DEBUG for spammy debug prints.
 * Include header in any number of source files.
 *
 * 1. telnet_create() to allocate the state handle.
 * 2. acquire data from your network libraries (read(), recv())
 * 3. telnet_begin() to point to the current working buffer
 * 4. telnet_continue() to check if data is left in the working buffer.
 * 5. telnet_gettext() to get normal text from the stream.
 * 6. telnet_getcontrol() to get WILL/WONT/DO/DONT/SB options and sub-option data.
 * 7. telnet_end() to stop pointing to the working buffer.
 * 8. when complete, free data with telnet_free()
 *
 */
/* BUGS & TODO:
 * + handle TCP Urgent/OOB state changes (like SYNCH)
 */
/* EXAMPLE CODE:
 * #define JDM_TELNET_IMPLEMENTATION
 * // #define JDM_TELNET_DEBUG
 * #include "jdm_telnet.h"
 *
 * #include <stdlib.h>
 *
 * #include <arpa/inet.h>
 * #include <sys/select.h>
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <unistd.h>
 *
 * struct client {
 *     int fd; // 0 means client is not valid / unused
 *     struct telnet_info *ts;
 * };
 *
 * static struct client *clients;
 * static int max_clients = 400;
 *
 * int main()
 * {
 *     int e;
 *     int port = 3000;
 *
 *     // initialize clients
 *     clients = calloc(max_clients, sizeof(*clients));
 *
 *     // setup server's listen socket
 *     int server_fd = socket(AF_INET, SOCK_STREAM, 0);
 *     if (server_fd < 0)
 *         return perror("socket()"), 1;
 *     struct sockaddr_in listenaddr = {
 *         .sin_addr.s_addr = INADDR_ANY,
 *         .sin_port = htons(port),
 *     };
 *     e = bind(server_fd, (struct sockaddr*)&listenaddr, sizeof(listenaddr));
 *     if (e)
 *         return perror("bind()"), 1;
 *     listen(server_fd, 6);
 *     printf("Listening on *:%u\n", port);
 *
 *     // server's listen loop
 *     int maxfd;
 *     fd_set rfds;
 *     int i;
 *
 *     while (1) {
 *         // initialize activate clients
 *         FD_ZERO(&rfds);
 *         maxfd = server_fd;
 *         for (i = 0; i < max_clients; i++) {
 *             if (!clients[i].fd)
 *                 continue; // ignore unused clients
 *             FD_SET(i, &rfds);
 *             if (i > maxfd)
 *                 maxfd = i;
 *         }
 *         FD_SET(server_fd, &rfds);
 *
 *         // wait for server activity
 *         e = select(maxfd + 1, &rfds, NULL, NULL, NULL);
 *         if (e < 0)
 *             perror("select()");
 *         if (FD_ISSET(server_fd, &rfds)) { // accept new connection...
 *             struct sockaddr_in addr;
 *             socklen_t len = sizeof(addr);
 *             int newfd = accept(server_fd, (struct sockaddr*)&addr, &len);
 *
 *             if (newfd < 0) {
 *                 perror("accept");
 *             } else if (newfd >= max_clients) {
 *                 close(newfd);
 *                 fprintf(stderr, "ignored connection - too many clients!\n");
 *             } else {
 *                 struct client *cl = &clients[newfd];
 *                 cl->ts = telnet_create(80);
 *                 cl->fd = newfd;
 *                 printf("[%d] new connection\n", newfd);
 *             }
 *         } else { // process input from a client connection
 *             char buf[128];
 *             ssize_t buf_len;
 *             for (i = 0; i < max_clients; i++) {
 *                 struct client *cl = &clients[i];
 *
 *                 if (cl->fd <= 0)
 *                     continue; // not a valid client
 *
 *                 if (!FD_ISSET(i, &rfds))
 *                     continue; // no data
 *
 *                 // read buffer
 *                 buf_len = read(cl->fd, buf, sizeof(buf));
 *                 if (buf_len == 0) { // closed?
 *                     telnet_free(cl->ts);
 *                     cl->ts = NULL;
 *                     close(cl->fd);
 *                     cl->fd = 0;
 *                     continue;
 *                 }
 *
 *                 // process TELNET codes
 *                 telnet_begin(cl->ts, buf_len, buf);
 *                 while (telnet_continue(cl->ts)) {
 *                     const char *text;
 *                     size_t text_len = 123;
 *                     if (telnet_gettext(cl->ts, &text_len, &text)) {
 *                         printf("[%d] len=%d text=\"%.*s\"\n", cl->fd, (int)text_len, (int)text_len, text);
 *                         // TODO: implement something interesting...
 *                     }
 *                     unsigned char command, option;
 *                     const unsigned char *extra;
 *                     size_t extra_len;
 *                     if (telnet_getcontrol(cl->ts, &command, &option, &extra_len, &extra)) {
 *                         printf("[%d] command=%u option=%u len=%d\n", cl->fd, command, option, (int)extra_len);
 *                         // TODO: implement something interesting...
 *                     }
 *                 }
 *
 *                 telnet_end(cl->ts);
 *             }
 *         }
 *     }
 *
 *     return 0;
 * }
 */
/* References & Further Reading:
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
#ifndef JDM_TELNET_H_
#define JDM_TELNET_H_
#include <stddef.h>

struct telnet_info;
/* extra_max controls buffer for Subnegotiation */
struct telnet_info *telnet_create(size_t extra_max);
int telnet_begin(struct telnet_info *ts, size_t inbuf_len, const char *inbuf);
int telnet_gettext(struct telnet_info *ts, size_t *len, const char **ptr);
int telnet_getcontrol(struct telnet_info *ts, unsigned char *command, unsigned char *option, size_t *extra_len, const  unsigned char **extra);
int telnet_continue(struct telnet_info *ts);
int telnet_end(struct telnet_info *ts);
void telnet_free(struct telnet_info *ts);

#ifdef JDM_TELNET_IMPLEMENTATION
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TELCMDS
#define TELOPTS

#include <arpa/telnet.h>

enum telnet_state {
    TelnetStateError,           /* some error occurred and we cannot proceed */
    TelnetStateText,
    TelnetStateIacIac,          /* IAC escape */
    TelnetStateIacCommand,      /* WILL WONT DO DONT ... */
    TelnetStateIacOption,       /* TELOPT_xxx */
    TelnetStateSb,              /* SB ... */
    TelnetStateSbIac,           /* IAC inside Sb */
};

struct telnet_info {
    enum telnet_state telnet_state;
    const unsigned char *inbuf;
    size_t inbuf_len, inbuf_current;
    unsigned char command, option;
    size_t extra_len;
    size_t extra_max;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
    unsigned char extra[];
#else
    unsigned char extra[0]; /* hack to do flex arrays in C89 */
#endif
};

struct telnet_info *telnet_create(size_t extra_max) {
    struct telnet_info *ret;
    /* if extra_max is 0 pick a reasonable size */
    if(!extra_max) extra_max=48;
    ret=malloc(sizeof *ret + extra_max);
    ret->telnet_state=TelnetStateText;
    ret->extra_len=0;
    ret->extra_max=extra_max;
    ret->inbuf_len=0;
    ret->inbuf_current=0;
    ret->inbuf=NULL;
    return ret;
}

/* loads a buffer to the telnet engine */
int telnet_begin(struct telnet_info *ts, size_t inbuf_len, const char *inbuf) {
    assert(ts != NULL);
    assert(ts->inbuf == NULL);
    ts->inbuf=(const unsigned char*)inbuf;
    ts->inbuf_len=inbuf_len;
    ts->inbuf_current=0;
    return 1;
}

#ifdef JDM_TELNET_DEBUG
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
    assert(ts->inbuf != NULL);
    assert(ptr != NULL);
    assert(len != NULL);

    if(ts->telnet_state == TelnetStateError || (ts->inbuf_current >= ts->inbuf_len)) {
        /* no more data */
        return 0;
    }

    switch(ts->telnet_state) {
        case TelnetStateError:
            return 0;
        case TelnetStateIacIac:
        case TelnetStateText:
            *ptr=(const char*)ts->inbuf+ts->inbuf_current;
            current=ts->inbuf_current;
            newlen=0;
            if(ts->telnet_state==TelnetStateIacIac) {
                ts->telnet_state=TelnetStateText;
                /* skip the IAC in the loop below */
                current++;
                newlen++;
            }
            for(;current<ts->inbuf_len;current++,newlen++) {
                    if(ts->inbuf[current]==IAC) {
                        ts->telnet_state=TelnetStateIacCommand;
#ifdef JDM_TELNET_DEBUG
                        fprintf(stderr, "command: ");
                        hexdump(8, &ts->inbuf[current]);
                        fprintf(stderr, "\n");
#endif
                        current++;
                        break;
                    }
            }
#ifdef JDM_TELNET_DEBUG
            fprintf(stderr, "curr: %d len: %d inbuf: %d %d\n",
                        (int)current, (int)newlen,
                        (int)ts->inbuf_current, (int)ts->inbuf_len);
#endif
            *len=newlen;
            ts->inbuf_current=current;
            return 1;
        case TelnetStateIacCommand:
        case TelnetStateIacOption:
        case TelnetStateSb:
        case TelnetStateSbIac:
            return 0;
    }
    fprintf(stderr, "Invalid telnet state %d in %p\n", ts->telnet_state, (void*)ts);
    ts->telnet_state=TelnetStateError;
    return 0;
}

/* get the next control item from the telnet engine.
 * ts - telnet info/state
 * command - pointer to a single unsigned char
 * option - pointerto a single unsigned char
 * extra_len - pointer to write the length of the extra data
 * extra - extra data buffer (for SB)
 */
int telnet_getcontrol(struct telnet_info *ts, unsigned char *command, unsigned char *option, size_t *extra_len, const  unsigned char **extra) {
    unsigned char tmp;

    assert(ts != NULL);
    assert(ts->inbuf != NULL);
    assert(command != NULL);
    assert(option != NULL);

again:
    if(ts->telnet_state == TelnetStateError || (ts->inbuf_current >= ts->inbuf_len)) {
        /* no more data */
        return 0;
    }

    switch(ts->telnet_state) {
        case TelnetStateError:
            return 0;
        case TelnetStateIacCommand:
            /* look for command parameter to IAC */
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current];
#ifdef JDM_TELNET_DEBUG
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
                    ts->command=tmp;
                    ts->inbuf_current++;
                    ts->telnet_state=TelnetStateText; /* go back to text */
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
                    ts->command=tmp;
                    ts->inbuf_current++;
                    ts->telnet_state=TelnetStateText; /* go back to text */
                    ts->option=0;
                    if(command) *command=ts->command;
                    if(option) *option=0;
                    if(extra_len) *extra_len=0;
                    if(extra) *extra=0;
                    return 1;
                case SB: /* Subnegotiation Begin */
                    /* format: IAC SB <option> ... IAC SE */
                    ts->command=SB;
                    ts->telnet_state=TelnetStateSb;
                    ts->extra_len=0;
                    ts->inbuf_current++;
                    goto again;
                case SE: /* Subnegotiation End */
                    /* this is an error in this state. we ignore it */
                    ts->telnet_state=TelnetStateText;
                    ts->inbuf_current++; /* swallow the sequence code */
#ifdef JDM_TELNET_DEBUG
                    fprintf(stderr, "Found IAC SE in outside of SB stream, ignoring it.\n");
#endif
                    goto again;
                default: /* treat anything we don't know as a 2 byte code */
                    /* the following is for 2-byte IAC <cmd> codes */
#ifdef JDM_TELNET_DEBUG
                    fprintf(stderr, "unknown code %u\n", tmp);
#endif
                    ts->command=tmp;
                    ts->inbuf_current++;
                    ts->telnet_state=TelnetStateText; /* go back to text */
                    ts->option=0;
                    if(command) *command=ts->command;
                    if(option) *option=0;
                    if(extra_len) *extra_len=0;
                    if(extra) *extra=0;
                    return 1;
            }
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
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current++];
            if(tmp==IAC) {
                /* found IAC */
                ts->telnet_state=TelnetStateSbIac;
            } else {
                /* append data to buffer */
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=tmp;
                }
            }
            goto again;
        case TelnetStateSbIac:
            /* TODO: look for IAC SE */
            tmp=(unsigned char)ts->inbuf[ts->inbuf_current++];
            if(tmp==IAC) {
                /* IAC IAC escape in SB sequence */
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=tmp;
                }
            } else if(tmp==SE) {
                /* IAC SE terminating SB sequence */
                ts->telnet_state=TelnetStateText;
                if(command) *command=ts->command;
                if(option) *option=ts->extra[0];
                if(extra_len) *extra_len=ts->extra_len;
                if(extra) *extra=ts->extra;
                return 1;
            } else {
                /* something unknown. don't escape anything */
                if(ts->extra_len < ts->extra_max) {
                    ts->extra[ts->extra_len++]=IAC;
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
    fprintf(stderr, "Invalid telnet state %d in %p\n", ts->telnet_state, (void*)ts);
    ts->telnet_state=TelnetStateError;
    return 0;
}

/* returns true while telnet_getXXX() can still be called */
int telnet_continue(struct telnet_info *ts) {
    return ts->telnet_state == TelnetStateError || (ts->inbuf_current < ts->inbuf_len);
}

/* finishes an update cycle. the buffer passed as telnet_begin is no longer
 * referenced after this function.
 * return 0 if the buffer still had data in it (unconsumed data error)
 * return 1 on success
 */
int telnet_end(struct telnet_info *ts) {
    int result=1;

    /* check that the buffer was completely consumed */
    if(ts->inbuf_current<ts->inbuf_len) {
#ifdef JDM_TELNET_DEBUG
        fprintf(stderr, "Unconsumed data!\n");
#endif
        result=0;
    }

    ts->inbuf=0;
    ts->inbuf_len=0;
    return result;
}

/* releases the telnet state */
void telnet_free(struct telnet_info *ts) {
    free(ts);
}
#endif /* JDM_TELNET_IMPLEMENTATION */
#endif /* JDM_TELNET_H_ */
