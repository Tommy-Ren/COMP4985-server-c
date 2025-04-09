#include "../include/message.h"
#include "../include/account.h"
#include "../include/chat.h"
#include "../include/network.h"
#include "../include/user_db.h"
#include "../include/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint16_t user_count = 0;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
uint32_t msg_count  = MAX_MSG;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
int      user_index = 0;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

static void handle_sm_diagnostic(char *msg);
/* Declaration for static functions */
static ssize_t handle_message(message_t *message);
static ssize_t handle_package(message_t *message);
static ssize_t handle_header(message_t *message, ssize_t nread);
static ssize_t handle_payload(message_t *message, ssize_t nread);
static ssize_t handle_response(message_t *message);
// static ssize_t     send_response(message_t *message);
static void        send_sm_response(char *msg);
static ssize_t     send_error_response(message_t *message);
static const char *error_code_to_string(const error_code_t *code);
static void        count_user(const int *client_id);

/* Error code map */
static const error_code_map code_map[] = {
    {EC_GOOD,          ""                      },
    {EC_INV_USER_ID,   "Invalid User ID"       },
    {EC_INV_AUTH_INFO, "Invalid Authentication"},
    {EC_USER_EXISTS,   "User Already Exist"    },
    {EC_SERVER,        "Server Error"          },
    {EC_INV_REQ,       "Invalid message"       },
    {EC_REQ_TIMEOUT,   "message Timeout"       }
};

void handle_connections(int server_fd)
{
    /* Use the global server_running variable declared in utils.h */
    struct pollfd fds[MAX_FDS];
    int           client_id[MAX_FDS];
    char          db_name[] = "meta_db";
    DBO           meta_db;
    int           poll_count;
    message_t     message;
    char          sm_msg[MESSAGE_NUM];
    int           i;
    ssize_t       retval;

    // Initialize fds
    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;
    for(i = 1; i < MAX_FDS; i++)
    {
        fds[i].fd    = -1;
        client_id[i] = -1;
    }

    // Initialize meta database
    meta_db.name = db_name;
    if(init_pk(&meta_db, "USER_PK") < 0)
    {
        perror("Failed to initialize meta_db\n");
        goto exit;
    }
    if(database_open(&meta_db) < 0)
    {
        perror("Failed to open meta_db");
        goto exit;
    }

    handle_sm_diagnostic(sm_msg);

    /* Global server_running will be used here */
    while(server_running)
    {
        errno      = 0;
        poll_count = poll(fds, MAX_FDS, TIMEOUT);

        // Poll for events on the file descriptors
        if(poll_count < 0)
        {
            if(errno == EINTR)
            {
                goto exit;
            }
            perror("poll error");
            goto exit;
        }
        // On poll timeout, update user count and send diagnostics if connected to server manager.
        if(poll_count == 0)
        {
            printf("poll timeout\n");
            if(store_int(meta_db.db, "USER_PK", user_index) != 0)
            {
                perror("update user_index");
                goto exit;
            }
            count_user(client_id);
            if(sm_fd >= 0)    // Only send diagnostic update if connected to the server manager.
            {
                send_sm_response(sm_msg);
            }
            continue;
        }
        // Check for new client connections
        if(fds[0].revents & POLLIN)
        {
            int                     client_added;
            int                     client_fd;
            struct sockaddr_storage client_addr;
            socklen_t               client_addr_len = sizeof(client_addr);
            int                     flags;

            client_added = 0;

            client_fd = socket_accept(server_fd, &client_addr, &client_addr_len);
            if(client_fd < 0)
            {
                if(errno == EINTR)
                {
                    server_running = 0;
                    goto exit;
                }
                perror("accept error");
                continue;
            }

            flags = fcntl(client_fd, F_GETFL, 0);
            if(flags == -1)
            {
                perror("fcntl (get flags) error");
                close(client_fd);
                continue;
            }
            if(fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                perror("fcntl (set non-blocking) error");
                close(client_fd);
                continue;
            }

            for(i = 1; i < MAX_FDS; i++)
            {
                if(fds[i].fd == -1)
                {
                    fds[i].fd     = client_fd;
                    fds[i].events = POLLIN;
                    client_id[i]  = client_fd;    // Use the accepted socket fd as the temporary client ID
                    client_added  = 1;
                    break;
                }
            }

            if(!client_added)
            {
                printf("Too many clients connected. Rejecting connection.\n");
                close(client_fd);
                continue;
            }
        }

        // Handle incoming data on existing client connections.
        for(i = 1; i < MAX_FDS; i++)
        {
            if(fds[i].fd != -1)
            {
                if(fds[i].revents & POLLIN)
                {
                    printf("polling client#%d\n", client_id[i]);
                    message.fds          = fds;
                    message.client       = &fds[i];
                    message.client_id    = &client_id[i];
                    message.payload_len  = HEADERLEN;
                    message.response_len = 3;
                    message.code         = EC_GOOD;

                    retval = 0;    // reset retval for each connection
                    while(retval != END)
                    {
                        printf("handling message");
                        retval = handle_message(&message);
                    }
                    /* Do not exit the poll loop—continue to process other connections */
                }
                if(fds[i].revents & (POLLHUP | POLLERR))
                {
                    printf("client#%d disconnected.\n", client_id[i]);
                    close(fds[i].fd);
                    fds[i].fd     = -1;
                    fds[i].events = 0;
                    client_id[i]  = -1;
                    continue;
                }
            }
        }
    }

    // Sync the user database
    if(store_int(meta_db.db, "USER_PK", user_index) != 0)
    {
        perror("Failed to sync user database");
        goto exit;
    }

exit:
    store_int(meta_db.db, "USER_PK", user_index);
    dbm_close(meta_db.db);
}

static ssize_t handle_message(message_t *message)
{
    ssize_t retval;

    /* Allocate the message buffer */
    message->req_buf = malloc(HEADERLEN);
    if(!message->req_buf)
    {
        perror("Failed to allocate message message buffer");
        retval = -1;
        goto exit;
    }

    /* Allocate the response buffer */
    message->res_buf = malloc(RESPONSELEN);
    if(!message->res_buf)
    {
        perror("Failed to allocate message response buffer");
        retval = -2;
        goto exit;
    }
    memset(message->res_buf, 0, RESPONSELEN);

    retval = handle_package(message);

exit:
    sfree(&message->req_buf);
    sfree(&message->res_buf);
    return retval;
}

/* BER Decoder function */
static ssize_t handle_package(message_t *message)
{
    ssize_t retval;
    ssize_t nread;
    void   *tmp;

    nread = read(message->client->fd, (char *)message->req_buf, message->payload_len);
    if(nread < 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // No data yet; not a real error for non-blocking sockets
            return 0;    // Return NO_DATA or just 0 to skip this round
        }
        perror("Fail to read header");
        message->code = EC_SERVER;
        return -1;
    }

    printf("handling header\n");
    if(handle_header(message, nread) < 0)
    {
        perror("Failed to decode header");
        return -2;
    }

    // Reallocate buffer to fit the payload
    tmp = realloc(message->req_buf, (size_t)(message->payload_len + HEADERLEN));
    if(!tmp)
    {
        perror("Failed to reallocate message body\n");
        free(message->req_buf);
        message->code = EC_SERVER;
        return -3;
    }
    message->req_buf = tmp;

    // Read payload_length into buffer
    nread = read(message->client->fd, ((char *)message->req_buf) + HEADERLEN, message->payload_len);
    if(nread < 0)
    {
        perror("Failed to read message body\n");
        message->code = EC_SERVER;
        return -4;
    }

    printf("handling payload\n");
    retval = handle_payload(message, nread);
    if(retval == ACCOUNT_ERROR)
    {
        perror("Failed to identify account package\n");
        return ACCOUNT_ERROR;
    }
    if(retval == ACCOUNT_LOGIN_ERROR)
    {
        perror("Failed to login\n");
        return ACCOUNT_LOGIN_ERROR;
    }
    if(retval == ACCOUNT_CREATE_ERROR)
    {
        perror("Failed to create account\n");
        return ACCOUNT_CREATE_ERROR;
    }
    if(retval == ACCOUNT_EDIT_ERROR)
    {
        perror("Failed to edit account\n");
        return ACCOUNT_EDIT_ERROR;
    }
    if(retval == CHAT_ERROR)
    {
        perror("Chat error\n");
        return CHAT_ERROR;
    }
    if(retval == END)
    {
        perror("End, closing client fd.\n");
        return END;
    }

    return 0;
}

static ssize_t handle_header(message_t *message, ssize_t nread)
{
    char *buf;

    buf = (char *)message->req_buf;
    if(nread < (ssize_t)sizeof(message->payload_len))
    {
        perror("Payload length mismatch\n");
        message->code = EC_INV_REQ;
        return -1;
    }

    memcpy(&message->type, buf, sizeof(message->type));
    buf += sizeof(message->type);

    memcpy(&message->version, buf, sizeof(message->version));
    buf += sizeof(message->version);

    memcpy(&message->sender_id, buf, sizeof(message->sender_id));
    buf += sizeof(message->sender_id);
    message->sender_id = ntohs(message->sender_id);

    memcpy(&message->payload_len, buf, sizeof(message->payload_len));
    message->payload_len = ntohs(message->payload_len);

    // DEBUG
    printf("Header type: %d\n", (int)message->type);
    printf("Header version: %d\n", (int)message->version);
    printf("Header sender_id: %d\n", (int)message->sender_id);
    printf("Header payload_len: %d\n", (int)message->payload_len);

    return 0;
}

static ssize_t handle_payload(message_t *message, ssize_t nread)
{
    ssize_t retval;

    if(nread < (ssize_t)message->payload_len)
    {
        printf("Payload length mismatch");
        message->code = EC_INV_REQ;
        return ACCOUNT_ERROR;
    }

    printf("handle payload\n");
    switch(message->type)
    {
        case ACC_LOGIN:
        case ACC_CREATE:
        case ACC_EDIT:
        case ACC_LOGOUT:
            retval = account_handler(message);
            if(retval < 0)
            {
                send_error_response(message);
                return retval;
            }
            break;

        case CHT_SEND:
            retval = chat_handler(message);
            if(retval < 0)
            {
                send_error_response(message);
                return retval;
            }
            break;

        default:
            message->code = EC_INV_REQ;
            send_error_response(message);
            return ACCOUNT_ERROR;
    }

    // If no error, return 0 always
    return handle_response(message);
}

static ssize_t handle_response(message_t *message)
{
    if(message->type != CHT_SEND)
    {
        message->response_len = (uint16_t)(HEADERLEN + ntohs(message->response_len));
        printf("response_len: %d\n", (message->response_len));
        write(message->client->fd, message->res_buf, message->response_len);
    }

    sfree(&message->req_buf);
    return 0;
}

static void count_user(const int *client_id)
{
    user_count = 0;
    for(int i = 1; i < MAX_FDS; i++)
    {
        printf("user id: %d\n", client_id[i]);
        if(client_id[i] != -1)
        {
            user_count++;
        }
    }
    printf("Current number of users: %d\n", user_count);
}

static void handle_sm_diagnostic(char *msg)
{
    char    *ptr = msg;
    uint16_t temp;

    /* Build the 4-byte header for server management */
    *ptr++ = SVR_DIAGNOSTIC;                /* Packet type, from your sm_type_t enum (0x0A) */
    *ptr++ = VERSION_NUM;                   /* Protocol version */
    temp   = htons(DIAGNOSTIC_PAYLOAD_LEN); /* 10 bytes payload */
    memcpy(ptr, &temp, sizeof(temp));
    ptr += sizeof(temp);

    /* Append the diagnostic payload */
    /* First BER block: user_count */
    *ptr++ = BER_INT;          /* BER tag (should be 0x02) */
    *ptr++ = sizeof(uint16_t); /* Length: 2 bytes */
    temp   = htons(user_count);
    memcpy(ptr, &temp, sizeof(temp));
    ptr += sizeof(uint16_t);

    /* Second BER block: msg_count */
    *ptr++ = BER_INT;          /* BER tag */
    *ptr++ = sizeof(uint32_t); /* Length: 4 bytes */
    {
        uint32_t temp32 = htonl(msg_count);
        memcpy(ptr, &temp32, sizeof(temp32));
    }
    /* Total bytes written = SM_HEADERLEN (4) + DIAGNOSTIC_PAYLOAD_LEN (10) = 14 bytes */
}

/* Send the diagnostic message to the server manager.
   Since the server manager protocol uses a 4-byte header,
   we update the payload fields using SM_HEADERLEN as the starting offset.
   We no longer attempt to read any response.
*/
static void send_sm_response(char *msg)
{
    char    *ptr = msg;
    uint16_t net_uc;
    uint32_t net_mc;

    /* Update the diagnostic payload values */
    /* Skip SM_HEADERLEN (4 bytes) and the first BER tag and length (2 bytes)
       so that we are at offset 6 (4+2) where the user_count value (2 bytes) should be.
    */
    ptr += SM_HEADERLEN + 2;
    net_uc = htons(user_count);
    memcpy(ptr, &net_uc, sizeof(net_uc));

    /* Skip the user_count value (2 bytes) and the next BER tag and length (2 bytes)
       to locate the msg_count value (offset 10 in the packet).
    */
    ptr += sizeof(uint16_t) + 2;
    net_mc = htonl(msg_count);
    memcpy(ptr, &net_mc, sizeof(net_mc));

    printf("Sending user count to server manager\n");
    if(write(sm_fd, msg, DIAGNOSTIC_MSG_LEN) < 0)
    {
        perror("Failed to send user count");
    }
    /* We do not attempt to read any response because the manager protocol
       currently does not send an acknowledgment for diagnostic updates.
    */
}

static ssize_t send_error_response(message_t *message)
{
    char    *ptr;
    uint16_t sender_id;
    uint8_t  msg_len;

    sender_id = SYSID;
    ptr       = (char *)message->res_buf;

    if(message->type != ACC_LOGOUT)
    {
        const char *msg;

        // Build error response header.
        printf("sending error response\n");
        *ptr++    = SYS_ERROR;
        *ptr++    = VERSION_NUM;
        sender_id = htons(sender_id);
        memcpy(ptr, &sender_id, sizeof(sender_id));
        ptr += sizeof(sender_id);
        msg     = error_code_to_string(&message->code);
        msg_len = (uint8_t)strlen(msg);
        // Update response_len to include tag fields and message.
        message->response_len = (uint16_t)(message->response_len + (sizeof(uint8_t) + sizeof(uint8_t) + msg_len));
        message->response_len = htons(message->response_len);
        memcpy(ptr, &message->response_len, sizeof(message->response_len));
        ptr += sizeof(message->response_len);
        // Encode error code and error string.
        *ptr++ = BER_INT;
        *ptr++ = sizeof(uint8_t);
        memcpy(ptr, &message->code, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
        *ptr++ = BER_STR;
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        memcpy(ptr, msg, msg_len);
        // Calculate the final response length.
        message->response_len = (uint16_t)(HEADERLEN + ntohs(message->response_len));
    }

    if(write(message->client->fd, message->res_buf, message->response_len) < 0)
    {
        perror("Failed to send error response");
        sfree(&message->req_buf);
        return -1;
    }

    printf("Response: %s\n", (char *)message->res_buf);
    sfree(&message->req_buf);

    return 0;
}

static const char *error_code_to_string(const error_code_t *code)
{
    size_t i;
    for(i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++)
    {
        if(code_map[i].code == *code)
        {
            return code_map[i].msg;
        }
    }
    return UNKNOWNTYPE;
}
