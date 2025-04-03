#include "../include/args.h"
#include "../include/network.h"
#include "../include/user_db.h"
#include "../include/utils.h"    // Declares setup_signal_handler() and server_running
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// #define SM_MAX_ATTEMPTS 5    // Maximum number of connection attempts to server manager
// #define SM_RETRY_DELAY 1     // Delay in seconds between attempts
#define SM_FD 3

Arguments global_args;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

int main(int argc, char *argv[])
{
    int sockfd;
    int sm_fd;    // Server manager socket descriptor

    memset(&global_args, 0, sizeof(Arguments));
    global_args.ip   = NULL;
    global_args.port = 0;

    parse_args(argc, argv, &global_args);

    printf("Listening on %s:%d\n", global_args.ip, global_args.port);

    // Set up signal handler
    setup_signal_handler();

    sockfd = server_tcp(&global_args);
    if(sockfd < 0)
    {
        perror("Failed to create server network.");
        exit(EXIT_FAILURE);
    }

    // === NEW!!! HARD CODED THE SERVER MANAGER FILE DESCRIPTOR BECAUSE THE SERVER STARTER HANDLES IT ===

    // // Try to connect to the server manager up to SM_MAX_ATTEMPTS times.
    sm_fd = SM_FD;

    // Start handling client connections (and optionally sending diagnostics to the server manager)
    handle_connections(sockfd, sm_fd);

    return EXIT_SUCCESS;
}
