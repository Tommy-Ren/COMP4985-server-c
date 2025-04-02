#ifndef ARGS_H
#define ARGS_H

#include <arpa/inet.h>

// struct to hold the arguments
typedef struct Arguments
{
    const char *ip;      // cppcheck-suppress unusedStructMember
    in_port_t   port;    // cppcheck-suppress unusedStructMember
    const char *sm_ip;   // cppcheck-suppress unusedStructMember
    in_port_t   sm_port; // cppcheck-suppress unusedStructMember
} Arguments;

// prints usage message and exits
_Noreturn void usage(const char *app_name, int exit_code, const char *message);

// checks arguments
void parse_args(int argc, char *argv[], Arguments *args);

#endif
