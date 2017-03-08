
#ifndef __NET_STUFF_H
#define __NET_STUFF_H

int open_tcp(char *hostname, unsigned short uport);
void close_tcp(int fd);
int read_write_ics_fd(int input_fd, int output_fd, int ics_fd);
void send_to_fics(int ics_fd, char *buff, int *rd);

#endif

