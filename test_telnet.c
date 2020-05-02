/**************************** TEST & EXAMPLE CODE ****************************/

#define JDM_TELNET_IMPLEMENTATION
#define JDM_TELNET_DEBUG
#include "jdm_telnet.h"

/* USAGE:
 *  + TODO - explain how to use it
 * IDEAS:
 *  + combine telnet_end() and telnet_continue() ?
 * TODO:
 *   + handle control messages and responses in the struct (an NVT driver?)
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
        { 12, "\377\364\377\365\377\366\377\367\377\370\377\371" }, /* IAC IP IAC AO IAC AYT IAC EC IAC EL IAC GA */
        { 3, "\377\361\377" }, /* IAC NOP IAC */
        { 2, "\376\42" }, /* DONT TELOPT_LINEMODE */
        { 15, "this is a test\n" },
        { 2, "\377\377" }, /* IAC IAC */
        { 4, "\33[0m" }, /* ESC [ 0 m  */
        { 2, "\377\377" }, /* IAC IAC */
        { 4, "\377\372\42\1" }, /* IAC SB LINEMODE MODE */
        { 3, "\1\377\360" }, /* EDIT IAC SE */
        { 4, "y\377\360x" }, /* y IAC SE x */
    };
    int i;
    struct telnet_info *ts;

    ts=telnet_create(0);
    for(i=0;i<(int)(sizeof(test_data)/sizeof(*test_data));i++) {
        telnet_begin(ts, test_data[i].n, test_data[i].b);
        while(telnet_continue(ts)) {
            const char *text_ptr;
            size_t text_len;
            const unsigned char *ex;
            size_t exlen;
            unsigned char cmd, opt;
            /* TODO: call telnet_getXXX in a loop until 0 */
            /* handle regular data */
            if(telnet_gettext(ts, &text_len, &text_ptr)) {
                /* Dump all normal text to stdout */
#ifndef NDEBUG
                fprintf(stderr, "text_len=%d\n", (int)text_len);
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
                if(TELCMD_OK(cmd)) { /* ignore the warning on this line */
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
