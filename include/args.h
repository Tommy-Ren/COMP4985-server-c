#ifndef ARGS_H
#define ARGS_H

#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define SERVER_MANAGER_IP "192.168.0.130"
#define PORT "8000"
#define SERVER_MANAGER_PORT "9000"

// struct to hold the arguments
typedef struct Arguments
{
    const char *ip;         // cppcheck-suppress unusedStructMember
    in_port_t   port;       // cppcheck-suppress unusedStructMember
    const char *sm_ip;      // cppcheck-suppress unusedStructMember
    in_port_t   sm_port;    // cppcheck-suppress unusedStructMember
} Arguments;

extern Arguments global_args;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

// prints usage message and exits
_Noreturn void usage(const char *app_name, int exit_code, const char *message);

// checks arguments
void parse_args(int argc, char *argv[], Arguments *args);

#endif
