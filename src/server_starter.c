/* Revised server_starter.c */
#include "../include/server_starter.h"
#include "../include/args.h"
#include "../include/network.h"
#include <fcntl.h> /* for fcntl */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SM_PORT 9000
#define MSG_LEN 4

/* Packet type definitions per our protocol */
#define SVR_START 0x14
#define SVR_STOP 0x15
#define SVR_ONLINE 0x0C
#define SVR_OFFLINE 0x0D

/* Use the protocol version defined in our message header */
#define PROTOCOL_VERSION VERSION_NUM

Arguments global_args;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

int main(int argc, char *argv[])
{
    int           sm_socket;
    int           child_pid = -1;
    unsigned char req[MSG_LEN];

    parse_args(argc, argv, &global_args);
    global_args.sm_port = SM_PORT;    // hard coded server manager port

    printf("Connecting to server manager at %s:%d\n", global_args.sm_ip, global_args.sm_port);
    sm_socket = server_manager_tcp(&global_args);
    if(sm_socket < 0)
    {
        fprintf(stderr, "Error connecting to server manager\n");
        exit(EXIT_FAILURE);
    }
    /* Set close-on-exec flag so that sm_socket is not inherited by the execv'd process */
    if(fcntl(sm_socket, F_SETFD, FD_CLOEXEC) == -1)
    {
        perror("fcntl FD_CLOEXEC");
    }
    printf("Connected to server manager.\n");

    /* Main loop: continuously wait for start/stop requests from the server manager */
    while(1)
    {
        ssize_t n = read(sm_socket, req, MSG_LEN);
        if(n <= 0)
        {
            perror("read");
            break;
        }
        if(n < MSG_LEN)
        {
            /* Incomplete message: wait for the complete message */
            continue;
        }
        /* Check that the protocol version matches */
        if(req[1] != PROTOCOL_VERSION)
        {
            fprintf(stderr, "Protocol version mismatch: expected %d, got %d\n", PROTOCOL_VERSION, req[1]);
            continue;
        }
        if(req[0] == SVR_START)
        {
            printf("Received SVR_Start request\n");
            if(child_pid <= 0)
            {
                child_pid = fork();
                if(child_pid < 0)
                {
                    perror("fork");
                    continue;
                }
                if(child_pid == 0)
                {
                    /* Child process: start the actual server */
                    printf("Starting server process...\n");
                    execv("./build/main", argv);
                    perror("execv");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    /* Parent process: send SVR_ONLINE response */
                    {
                        const unsigned char online_msg[MSG_LEN] = {SVR_ONLINE, PROTOCOL_VERSION, 0x00, 0x00};
                        if(write(sm_socket, online_msg, MSG_LEN) != MSG_LEN)
                        {
                            perror("write SVR_ONLINE");
                        }
                    }
                }
            }
            else
            {
                /* Server is already running; send online response */
                {
                    const unsigned char online_msg[MSG_LEN] = {SVR_ONLINE, PROTOCOL_VERSION, 0x00, 0x00};
                    if(write(sm_socket, online_msg, MSG_LEN) != MSG_LEN)
                    {
                        perror("write SVR_ONLINE");
                    }
                }
            }
        }
        else if(req[0] == SVR_STOP)
        {
            printf("Received SVR_Stop request\n");
            if(child_pid > 0)
            {
                {
                    const unsigned char offline_msg[MSG_LEN] = {SVR_OFFLINE, PROTOCOL_VERSION, 0x00, 0x00};
                    if(kill(child_pid, SIGINT) != 0)
                    {
                        perror("kill");
                    }
                    waitpid(child_pid, NULL, 0);
                    child_pid = -1;
                    if(write(sm_socket, offline_msg, MSG_LEN) != MSG_LEN)
                    {
                        perror("write SVR_OFFLINE");
                    }
                }
            }
            else
            {
                /* No server running; reply offline */
                {
                    const unsigned char offline_msg[MSG_LEN] = {SVR_OFFLINE, PROTOCOL_VERSION, 0x00, 0x00};
                    if(write(sm_socket, offline_msg, MSG_LEN) != MSG_LEN)
                    {
                        perror("write SVR_OFFLINE");
                    }
                }
            }
        }
        else
        {
            fprintf(stderr, "Received unknown request: 0x%02x\n", req[0]);
        }
    }
    close(sm_socket);
    return 0;
}
