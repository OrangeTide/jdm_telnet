#define JDM_TELNET_IMPLEMENTATION
// #define JDM_TELNET_DEBUG
#include "jdm_telnet.h"

#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

struct client {
	int fd; // 0 means client is not valid / unused
	struct telnet_info *ts;
};

static struct client *clients;
static int max_clients = 400;

int main()
{
	int e;
	int port = 3000;

	// initialize clients
	clients = calloc(max_clients, sizeof(*clients));

	// setup server's listen socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
		return perror("socket()"), 1;
	struct sockaddr_in listenaddr = {
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(port),
	};
	e = bind(server_fd, (struct sockaddr*)&listenaddr, sizeof(listenaddr));
	if (e)
		return perror("bind()"), 1;
	listen(server_fd, 6);
	printf("Listening on *:%u\n", port);

	// server's listen loop
	int maxfd;
	fd_set rfds;
	int i;

	while (1) {
		// initialize activate clients
		FD_ZERO(&rfds);
		maxfd = server_fd;
		for (i = 0; i < max_clients; i++) {
			if (!clients[i].fd)
				continue; // ignore unused clients
			FD_SET(i, &rfds);
			if (i > maxfd)
				maxfd = i;
		}
		FD_SET(server_fd, &rfds);

		// wait for server activity
		e = select(maxfd + 1, &rfds, NULL, NULL, NULL);
		if (e < 0)
			perror("select()");
		if (FD_ISSET(server_fd, &rfds)) { // accept new connection...
			struct sockaddr_in addr;
			socklen_t len = sizeof(addr);
			int newfd = accept(server_fd, (struct sockaddr*)&addr, &len);

			if (newfd < 0) {
				perror("accept");
			} else if (newfd >= max_clients) {
				close(newfd);
				fprintf(stderr, "ignored connection - too many clients!\n");
			} else {
				struct client *cl = &clients[newfd];
				cl->ts = telnet_create(80);
				cl->fd = newfd;
				printf("[%d] new connection\n", newfd);
			}
		} else { // process input from a client connection
			char buf[128];
			ssize_t buf_len;
			for (i = 0; i < max_clients; i++) {
				struct client *cl = &clients[i];

				if (cl->fd <= 0)
					continue; // not a valid client

				if (!FD_ISSET(i, &rfds))
					continue; // no data

				// read buffer
				buf_len = read(cl->fd, buf, sizeof(buf));
				if (buf_len == 0) { // closed?
					telnet_free(cl->ts);
					cl->ts = NULL;
					close(cl->fd);
					cl->fd = 0;
					continue;
				}

				// process TELNET codes
				telnet_begin(cl->ts, buf_len, buf);
				while (telnet_continue(cl->ts)) {
					const char *text;
					size_t text_len = 123;
					if (telnet_gettext(cl->ts, &text_len, &text)) {
						// TODO:
						printf("[%d] len=%d text=\"%.*s\"\n", cl->fd, (int)text_len, (int)text_len, text);
					}
					unsigned char command, option;
					const unsigned char *extra;
					size_t extra_len;
					if (telnet_getcontrol(cl->ts, &command, &option, &extra_len, &extra)) {
						printf("[%d] command=%u option=%u len=%d\n", cl->fd, command, option, (int)extra_len);
						// TODO:
					}
				}

				telnet_end(cl->ts);
			}
		}
	}

	return 0;
}
