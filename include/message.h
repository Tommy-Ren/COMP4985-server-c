#ifndef message_h
#define message_h

#include "../include/user_db.h"
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define SYSID (0)
#define VERSION_NUM (3)    // Updated to Protocol Version 3

#define MAX_FDS (5)
#define TIMEOUT (5000)

#define HEADERLEN (6)
#define U8ENCODELEN (3)
#define RESPONSELEN (256)
#define MESSAGE_NUM (100)
#define MESSAGE_LEN (14)
#define DIAGNOSTIC_PAYLOAD_LEN 0x000A

#define ACCOUNT_ERROR (1)
#define ACCOUNT_LOGIN_ERROR (2)
#define ACCOUNT_CREATE_ERROR (3)
#define ACCOUNT_EDIT_ERROR (4)
#define CHAT_ERROR (5)

#define UNKNOWNTYPE "Unknown Type"

extern uint16_t user_count;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern uint32_t msg_count;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern int      user_index;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

typedef enum
{
    BER_BOOL     = 0x01,
    BER_INT      = 0x02,
    BER_NULL     = 0x05,
    BER_ENUM     = 0x0A,
    BER_STR      = 0x0C,
    BER_SEQ      = 0x10,
    BER_SEQ_OF   = 0x30,
    BER_PRINTSTR = 0x13,
    BER_UTC_TIME = 0x17,
    BER_TIME     = 0x18,
} tag_t;

typedef enum
{
    // System
    SYS_SUCCESS = 0x00,
    SYS_ERROR   = 0x01,

    // Account
    ACC_LOGIN         = 0x0A,
    ACC_LOGIN_SUCCESS = 0x0B,
    ACC_LOGOUT        = 0x0C,
    ACC_CREATE        = 0x0D,
    ACC_EDIT          = 0x0E,

    // Chat
    CHT_SEND = 0x14,

    // List Users
    LST_GET      = 0x1E,
    LST_RESPONSE = 0x1F,

    // Group Chat
    GRP_JOIN   = 0x28,
    GRP_EXIT   = 0x29,
    GRP_CREATE = 0x2A,
} packet_t;

typedef enum
{
    // Success
    EC_GOOD = 0x00,

    // Client Errors
    EC_INV_USER_ID   = 0x0B,
    EC_INV_AUTH_INFO = 0x0C,
    EC_USER_EXISTS   = 0x0D,

    // Server Errors
    EC_SERVER = 0x15,

    // Validity Errors
    EC_INV_REQ     = 0x1F,
    EC_REQ_TIMEOUT = 0x20
} error_code_t;

/* Message*/
typedef struct message_t
{
    // Header (6 bytes)
    uint8_t  type;           // Packet type (1 byte)
    uint8_t  version;        // Protocol version (1 byte)
    uint16_t sender_id;      // Sender ID (2 bytes)
    uint16_t payload_len;    // Payload length (2 bytes)

    // Request
    void *req_buf;    // Request buffer

    // Resonse
    void    *res_buf;         // Response buffer
    uint16_t response_len;    // Response length

    // Shared
    error_code_t   code;         // Error code
    int            client_fd;    // Client file descriptor
    int           *client_id;    // Client ID
    struct pollfd *fds;          // Poll file descriptor
} message_t;

typedef struct
{
    error_code_t code;
    const char  *msg;
} error_code_map;

typedef enum
{
    // Manager
    MAN_SUCCESS = 0x00,
    MAN_ERROR   = 0x01,

    // Server Diagnostics
    SVR_DIAGNOSTIC = 0x0A,
    USR_ONLINE     = 0x0B,
    SVR_ONLINE     = 0x0C,
    SVR_OFFLINE    = 0x0D,

    // Server Commands
    SVR_START = 0x14,
    SVR_STOP  = 0x15,
} sm_type_t;

void handle_connections(int server_fd, int sm_fd);

#endif
