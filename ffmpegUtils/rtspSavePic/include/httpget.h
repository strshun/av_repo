#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
int http_conn(char const *ip_str,int port);
int http_send(char const*ip_str,int port,char *buf, int nbytes,char *reData);