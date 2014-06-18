
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <time.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define BSIZE 1024

static char *key = "Timestamp (FICS) v1.0 - programmed by Henrik Gram.";
static char hello[100] = "TIMESTAMP|cairo-board programmed by Julbra from FICS|Running on Gentoo Linux|";

/* encode the passed string using fics timeseal's protocol */
static int codec(char *s, int l) {

	int n;
	struct timeval tv;

	/* escape character which announces the start of the timestamp value */
	s[l++] = '\x18';

	/* get the current timestamp */
	gettimeofday(&tv, NULL);

	/* add the timestamp the the sent string */
	l += sprintf(&s[l], "%ld", (tv.tv_sec%10000)*1000 + tv.tv_usec/1000);

	/* escape character which announces the end of the timestamp value */
	s[l++]='\x19';

	/* padd the sent string with 1 till we reach a length multiple of 12 */
	for( ; l%12; l++) {
		s[l]='1';
	}

	/* Xor various characters with each other
	 * This is reversible and is why the server can decode our message*/
#define SC(A,B) s[B]^=s[A]^=s[B],s[A]^=s[B]
	for (n = 0; n < l; n += 12) {
		SC(n,n+11), SC(n+2,n+9), SC(n+4,n+7);
	}

	for(n = 0; n < l; n++) {
		s[n] = ((s[n]|0x80)^key[n%50]) - 32;
	}

	/* escape sequence announces the end of our message */
	s[l++] = '\x80';
	s[l++] = '\x0a';

	return l;
}

static void write_to_fd(int fd, char *buff, int n) {
	if(write(fd, buff, n) == -1) {
		perror(NULL);
		return;
	}
}

int open_tcp(char *hostname, unsigned short uport) {

	int socket_fd, i;
	struct hostent* host_info;
	struct sockaddr_in sa;

	socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_fd < 0) {
		perror(NULL);
		return -1;
	}

	host_info = gethostbyname(hostname);
	if (host_info == NULL) {
		perror(NULL);
		return -1;
	}

	memcpy(&sa.sin_addr.s_addr, host_info->h_addr, host_info->h_length);
	sa.sin_port = htons(uport);
	sa.sin_family = AF_INET;
	if (connect(socket_fd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
		perror(NULL);
		return -1;
	}

	i = codec(hello, strlen(hello));
	write_to_fd(socket_fd, hello, i);

	return socket_fd;
}

void close_tcp(int fd) {
	close(fd);
}

void send_to_fics(int ics_fd, char *buff, int *rd) {

	/* static storage duration, no linkage */
	static int i = 0;

	for( ; i < *rd; i++) {

		/* search for '\n' character */
		if (buff[i] == '\n') {
			char ffub[BSIZE+20];
			int k;

			/* copy passed buffer till '\n' */
			memcpy(ffub, buff, i);

			/* encode string */
			k = codec(ffub, i);

			/* send encoded data to socket */
			write_to_fd(ics_fd, ffub, k);

			/* fold back end of buffer to the beginning */
			for(i++, k = 0; i < *rd; i++, k++) {
				buff[k] = buff[i];
			}
			*rd = k;
			i = -1;
		}

	}
}

/* decode encrypted buff and write it to output_fd */
static void get_from_fics(int ics_fd, int output_fd, char *buff, int *rd) {

	int n, m;

	while(*rd > 0) {

		/* write ack to fics */
		if( ! strncmp(buff, "[G]\n\r", *rd < 5? *rd : 5)) {
			if(*rd < 5) {
				break;
			}
			else {
				char reply[20] = "\x2""9";
				n = codec(reply, 2);
				write_to_fd(ics_fd, reply, n);
				for(n = 5; n < *rd; n++) {
					buff[n-5] = buff[n];
				}
				*rd -= 5;
				continue;
			}
		}

		/* find '\r' in buff */
		for (n = 0; n <* rd && buff[n] != '\r'; n++);

		/* n < *rd means we found \r */
		if( n < *rd) {
			n++;
		}

		/* write the decoded data to output_fd */
		write_to_fd(output_fd, buff, n);

		for (m = n; m < *rd; m++) {
			buff[m-n] = buff[m];
		}
		*rd -= n;
	}
}

// Please optimise this
int read_write_ics_fd(int input_fd, int output_fd, int ics_fd) {

	int i;

	/* read and write file descriptor sets */
	fd_set r_fds;
	fd_set w_fds;

	/* reset our file descriptor sets */
	FD_ZERO(&r_fds);
	FD_ZERO(&w_fds);

	/* add input_fd to the read fds */
	FD_SET(input_fd, &r_fds);

	/* add ics_fd to both read and write fds */
	FD_SET(ics_fd, &r_fds);
	FD_SET(ics_fd, &w_fds);

	/* check I/O status of our fds */
	select(ics_fd+1, &r_fds, &w_fds, NULL, NULL);

	/* we can read from stdin and write to ics */
	if(FD_ISSET(input_fd, &r_fds) && FD_ISSET(ics_fd, &w_fds)) {
		static int w_rd = 0;
		static char buff[BSIZE];

		/* read from input_fd */
		w_rd += i = read(input_fd, buff+w_rd, BSIZE-w_rd);

		if(!i) {
			fprintf(stderr,"Read 0 bytes?!\n");
			return 1;
		}
		if(i < 0) {
			perror(NULL);
			return -1;
		}

		send_to_fics(ics_fd, buff, &w_rd);
		if(w_rd == BSIZE) {
			fprintf(stderr,"Line too long?!\n");
			return -1;
		}
	}

	/* we can read from and write to ics */
	if(FD_ISSET(ics_fd, &r_fds) && FD_ISSET(ics_fd, &w_fds)) {
		static int r_rd = 0;
		static char buff[BSIZE];

		/* read from ics */
		r_rd += i = read(ics_fd, buff, BSIZE-r_rd);
		if(!i) {
			fprintf(stderr, "Connection closed\n");
			return 1;
		}
		if(i < 0) {
			perror(NULL);
			return -1;
		}

		/* decode and write to output */
		get_from_fics(ics_fd, output_fd, buff, &r_rd);
		if(r_rd == BSIZE) {
			fprintf(stderr, "Receive buffer full?!\n");
			return -1;
		}
	}

	return 0;

}

int main_n(int argc, char **argv) {
	char *hostname;
	int port, ics_fd;

	if(argc == 3) {
		hostname = argv[1];
		port = atoi(argv[2]);
	} else if(argc == 2) {
		hostname = argv[1];
		port = 5000;
	} else {
		fprintf(stderr,"Usage:\n    %s ICS-host [ICS-port]\n",argv[0]);
		return 1;
	}

	ics_fd = open_tcp(hostname, port);
	if (ics_fd < 0) {
		fprintf(stderr, "Error connecting!\n");
		return 1;
	}

	while(!read_write_ics_fd(STDIN_FILENO, STDOUT_FILENO, ics_fd));

	return 0;
}

