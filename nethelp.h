#ifndef NETHELP_H
#define NETHELP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>  

#define LISTENQ 10  // Max pending connections in listen() backlog


int open_listenfd(int port);
int open_clientfd(char *hostname, int port);
int readline(int fd, char *buf, int maxlen);

#endif