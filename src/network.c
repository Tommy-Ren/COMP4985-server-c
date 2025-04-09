/* network.c */

#include "../include/network.h"
#include "../include/user_db.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void socket_setup(struct sockaddr_storage *addr, socklen_t *addr_len, const char *ip, in_port_t port);
static int  socket_create(int domain, int type, int protocol);
static int  socket_set(int sockfd);
/* Declare the parameters as pointers to const */
static int socket_bind(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len);
static int socket_listen(int server_fd, int backlog);
static int socket_connect(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len);

#define BASE_TEN 10
#define ERR_SOCKET (-1)
#define ERR_SET_OPTION (-2)
#define ERR_BIND (-3)
#define ERR_LISTEN (-4)
#define ERR_CONNECT (-5)

int sm_fd;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

/*
 * Function: server_tcp
 * Description: Sets up a TCP server using the provided arguments.
 * Returns: The server socket descriptor on success or an error code on failure.
 */
int server_tcp(const Arguments *args)
{
    int                     sockfd;
    struct sockaddr_storage addr;
    socklen_t               addr_len;

    addr_len = 0;
    memset(&addr, 0, sizeof(struct sockaddr_storage));

    /* Set up the address structure */
    socket_setup(&addr, &addr_len, args->ip, args->port);

    /* Create the socket */
    sockfd = socket_create(addr.ss_family, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Failed to create socket");
        return ERR_SOCKET;
    }

    /* Set socket options */
    if(socket_set(sockfd) < 0)
    {
        perror("Failed to set socket options");
        return ERR_SET_OPTION;
    }

    /* Bind the socket */
    if(socket_bind(sockfd, &addr, addr_len) < 0)
    {
        perror("Failed to bind socket");
        return ERR_BIND;
    }
    printf("Bound to socket: %s:%u\n", args->ip, args->port);
    /* Start listening */
    if(socket_listen(sockfd, SOMAXCONN) < 0)
    {
        perror("Failed to listen on socket");
        close(sockfd);
        return ERR_LISTEN;
    }
    printf("Listening on %s:%u\n", args->ip, args->port);

    return sockfd;
}

/*
 * Function: server_manager_tcp
 * Description: Connects to the server manager using the provided arguments.
 * Returns: The server manager socket descriptor on success or an error code on failure.
 */
int server_manager_tcp(const Arguments *args)
{
    struct sockaddr_storage addr;
    socklen_t               addr_len;

    addr_len = 0;
    memset(&addr, 0, sizeof(struct sockaddr_storage));

    /* Set up the address structure */
    socket_setup(&addr, &addr_len, args->sm_ip, args->sm_port);

    /* Create the socket */
    sm_fd = socket_create(addr.ss_family, SOCK_STREAM, 0);
    if(sm_fd < 0)
    {
        perror("Failed to create socket");
        return ERR_SOCKET;
    }

    /* Set socket options */
    if(socket_set(sm_fd) < 0)
    {
        perror("Failed to set socket options");
        return ERR_SET_OPTION;
    }

    /* Connect to the server manager */
    if(socket_connect(sm_fd, &addr, addr_len) < 0)
    {
        perror("Failed to connect to server manager");
        return ERR_CONNECT;
    }
    printf("Connected to server manager at %s:%u\n", args->sm_ip, args->sm_port);

    return sm_fd;
}

/* Set up address structure */
static void socket_setup(struct sockaddr_storage *addr, socklen_t *addr_len, const char *ip, in_port_t port)
{
    in_port_t net_port;
    net_port = htons(port);
    memset(addr, 0, sizeof(struct sockaddr_storage));

    /* Try to interpret the address as IPv4 */
    if(inet_pton(AF_INET, ip, &((struct sockaddr_in *)addr)->sin_addr) == 1)
    {
        struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)addr;
        ipv4_addr->sin_family         = AF_INET;
        ipv4_addr->sin_port           = net_port;
        *addr_len                     = sizeof(struct sockaddr_in);
    }
    /* If IPv4 fails, try interpreting it as IPv6 */
    else if(inet_pton(AF_INET6, ip, &((struct sockaddr_in6 *)addr)->sin6_addr) == 1)
    {
        struct sockaddr_in6 *ipv6_addr = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_family         = AF_INET6;
        ipv6_addr->sin6_port           = net_port;
        *addr_len                      = sizeof(struct sockaddr_in6);
    }
    else
    {
        fprintf(stderr, "%s is not a valid IPv4 or IPv6 address\n", ip);
        exit(EXIT_FAILURE);
    }
}

/* Create a socket */
static int socket_create(int domain, int type, int protocol)
{
    int sockfd = socket(domain, type, protocol);
    if(sockfd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

/* Set socket options */
static int socket_set(int sockfd)
{
    int flag = fcntl(sockfd, F_GETFL, 0);
    if(flag == -1)
    {
        perror("Failed to get flag");
        return -1;
    }

    // === NEW!!! NON-BLOCKING FLAG CAUSES ISSUES WITH THE SERVER STARTER SOCKET SET UP ===

    //    flag |= O_NONBLOCK;
    //    if(fcntl(sockfd, F_SETFL, flag) == -1)
    //    {
    //        perror("Failed to set flag");
    //        return -1;
    //    }

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        perror("Failed to set socket options");
        return -1;
    }
    return 0;
}

/* Bind the socket to an address.
   Note: We cast away the const qualifier because bind() requires a non-const pointer.
   We know that bind() does not modify the underlying address structure. */
static int socket_bind(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    int ret = bind(sockfd, (struct sockaddr *)addr, addr_len);
#pragma GCC diagnostic pop
    return ret;
}

/* Start listening on the socket */
static int socket_listen(int server_fd, int backlog)
{
    return listen(server_fd, backlog);
}

/* Connect the socket.
   Note: We cast away the const qualifier because connect() requires a non-const pointer.
   We know that connect() does not modify the underlying address structure. */
static int socket_connect(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    int ret = connect(sockfd, (struct sockaddr *)addr, addr_len);
#pragma GCC diagnostic pop
    return ret;
}

/* Accept a client connection */
int socket_accept(int server_fd, struct sockaddr_storage *client_addr, socklen_t *client_addr_len)
{
    int client_fd = accept(server_fd, (struct sockaddr *)client_addr, client_addr_len);
    if(client_fd < 0)
    {
        return -1;
    }
    return client_fd;
}

/* Convert port from string to in_port_t */
in_port_t convert_port(const char *binary_name, const char *str)
{
    char     *endptr;
    uintmax_t parsed_value = strtoumax(str, &endptr, BASE_TEN);
    if(errno != 0)
    {
        perror("Error parsing in_port_t");
        exit(EXIT_FAILURE);
    }
    if(*endptr != '\0')
    {
        usage(binary_name, EXIT_FAILURE, "Invalid characters in input.");
    }
    if(parsed_value < 1 || parsed_value > UINT16_MAX)
    {
        usage(binary_name, EXIT_FAILURE, "in_port_t value out of range.");
    }
    return (in_port_t)parsed_value;
}
