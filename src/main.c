#include "../include/args.h"
#include "../include/network.h"
#include "../include/user_db.h"
#include "../include/utils.h"    // Declares setup_signal_handler() and server_running
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int sockfd;

    memset(&global_args, 0, sizeof(Arguments));
    global_args.ip   = NULL;
    global_args.port = 0;

    parse_args(argc, argv);

    printf("Listening on %s:%d\n", global_args.ip, global_args.port);

    // Set up signal handler
    setup_signal_handler();

    sockfd = server_tcp(&global_args);
    if(sockfd < 0)
    {
        perror("Failed to create server network.");
        exit(EXIT_FAILURE);
    }

    // Start handling client connections (and optionally sending diagnostics to the server manager)
    handle_connections(sockfd);

    return EXIT_SUCCESS;
}
