#ifndef TELNET_H
#define TELNET_H
/* telnet.c */
struct telnet_info;
struct telnet_info *telnet_create(void);
int telnet_begin(struct telnet_info *ts, size_t inbuf_len, const char *inbuf);
int telnet_gettext(struct telnet_info *ts, size_t *len, const char **ptr);
int telnet_getcontrol(struct telnet_info *ts);
int telnet_continue(struct telnet_info *ts);
int telnet_end(struct telnet_info *ts);
void telnet_free(struct telnet_info *ts);
#endif
