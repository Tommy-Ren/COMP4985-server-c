#ifndef NETWORK_H
#define NETWORK_H

#include "../include/args.h"
#include "../include/message.h"
#include "../include/user_db.h"

extern int sm_fd;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

int server_tcp(const Arguments *args);
int server_manager_tcp(const Arguments *args);

int       socket_accept(int server_fd, struct sockaddr_storage *client_addr, socklen_t *client_addr_len);
in_port_t convert_port(const char *binary_name, const char *str);

#endif    // NETWORK_H
