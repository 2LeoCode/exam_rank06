#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

int SOCK_FD[__FD_SETSIZE];

int CL_ID[__FD_SETSIZE];
size_t CL_CNT = 0;

fd_set FDSET_ALL = { };
fd_set FDSET_RD, FDSET_WR;

char *READY_Q[__FD_SETSIZE] = { };
char *PENDING_Q[__FD_SETSIZE] = { };

void *ft_memcpy( void *dst, const void *src, size_t n ) {
	char *sdst = dst;
	const char *ssrc = src;

	while (n--)
		sdst[n] = ssrc[n];
	return dst;
}

void clear_queues( void ) {
	for (size_t i = 1; i <= CL_CNT; ++i) {
		free(READY_Q[SOCK_FD[i]]);
		free(PENDING_Q[SOCK_FD[i]]);
	}
}

void clear_sockets( void ) {
	for (size_t i = 0; i <= CL_CNT; ++i)
		close(SOCK_FD[i]);
}

void fatal( void ) {
	clear_queues();
	clear_sockets();
	write(2, "Fatal error\n", 12);
	exit(1);
}

void str_reserve( char **sp, size_t new_size ) {
	bool empty = !*sp;
	char *tmp = realloc(*sp, new_size);
	if (!tmp)
		fatal();
	if (empty)
		*tmp = 0;
	*sp = tmp;
}

void str_append( char **dst, char *src ) {
	str_reserve(dst, (*dst ? strlen(*dst) : 0) + strlen(src) + 1);
	strcat(*dst, src);
}

void append_to_queues( char **queue, char *s, size_t sender_pos ) {
	size_t i = 0;
	while (++i != sender_pos)
		str_append(&queue[SOCK_FD[i]], s);
	while (++i <= CL_CNT)
		str_append(&queue[SOCK_FD[i]], s);
}

void remove_client( int pos ) {
	FD_CLR(SOCK_FD[pos], &FDSET_ALL);
	close(SOCK_FD[pos]);

	char tmpbuf[128];
	sprintf(tmpbuf, "server: client %u just left\n", CL_ID[SOCK_FD[pos]]);
	append_to_queues(READY_Q, tmpbuf, pos);
	SOCK_FD[pos] = SOCK_FD[CL_CNT--];
}

int main( int argc, char **argv ) {
	if (argc < 2) {
		write(2, "Wrong number of arguments\n", 26);
		return 1;
	}

	struct sockaddr_in servaddr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(2130706433),
		.sin_port = htons(atoi(argv[1]))
	};
	uint32_t last_id = 0;

	SOCK_FD[0] = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCK_FD[0] == -1
	|| bind(SOCK_FD[0], (void*)&servaddr, sizeof(servaddr)) == -1
	|| listen(SOCK_FD[0], 32))
		fatal();

	FD_SET(SOCK_FD[0], &FDSET_ALL);
	while (true) {
		FDSET_RD = FDSET_WR = FDSET_ALL;
		if (select(__FD_SETSIZE, &FDSET_RD, &FDSET_WR, NULL, NULL) == -1)
			fatal();

		if (FD_ISSET(SOCK_FD[0], &FDSET_RD)) {
			SOCK_FD[++CL_CNT] = accept(SOCK_FD[0], NULL, NULL);
			if (SOCK_FD[CL_CNT] == -1)
				fatal();
			CL_ID[SOCK_FD[CL_CNT]] = last_id++;
			FD_SET(SOCK_FD[CL_CNT], &FDSET_ALL);

			char tmpbuf[128];
			sprintf(tmpbuf, "server: client %u just arrived\n", CL_ID[SOCK_FD[CL_CNT]]);
			append_to_queues(READY_Q, tmpbuf, CL_CNT);
		}

		for (size_t i = 1; i <= CL_CNT; ++i) {
			if (FD_ISSET(SOCK_FD[i], &FDSET_RD)) {
				char rdbuf[1024 + 1];
				const int rd_ret = recv(SOCK_FD[i], rdbuf, 1024, 0);

				if (rd_ret <= 0) {
					remove_client(i--);
					continue ;
				}
				rdbuf[rd_ret] = 0;
				append_to_queues(PENDING_Q, rdbuf, i);

				char tmpbuf[128];
				sprintf(tmpbuf, "client %u: ", CL_ID[SOCK_FD[i]]);
				char *nl;
				for (size_t j = 1; j <= CL_CNT; ++j) {
					if (i == j)
						continue ;
					while ((nl = strstr(PENDING_Q[SOCK_FD[j]], "\n"))) {
						const size_t len = nl - PENDING_Q[SOCK_FD[j]] + 1;
						char slice[len + 1];
						slice[len] = 0;

						ft_memcpy(slice, PENDING_Q[SOCK_FD[j]], len);
						str_append(&READY_Q[SOCK_FD[j]], tmpbuf);
						str_append(&READY_Q[SOCK_FD[j]], slice);


						strcpy(PENDING_Q[SOCK_FD[j]], nl + 1);
						str_reserve(&PENDING_Q[SOCK_FD[j]], strlen(PENDING_Q[SOCK_FD[j]]) + 1);
					}
				}
			}
			if (READY_Q[SOCK_FD[i]] && FD_ISSET(SOCK_FD[i], &FDSET_WR)) {
				const size_t len = strlen(READY_Q[SOCK_FD[i]]);
				const int wr_ret = send(SOCK_FD[i], READY_Q[SOCK_FD[i]], len, MSG_NOSIGNAL);
				if (wr_ret == -1) {
					remove_client(i--);
					continue ;
				}

				free(READY_Q[SOCK_FD[i]]);
				READY_Q[SOCK_FD[i]] = NULL;
			}
		}
	}
	return 0;
}
