
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#ifndef hci_server_h
#define hci_server_h

//==================================================================
//==================================================================
//  simple api for hci/l2cap

typedef uint8_t u8;
typedef uint16_t u16;

typedef struct {
    u8 b[6];
} __attribute__((packed)) bdaddr_t;

typedef struct
{
    bdaddr_t bdaddr;
    u8  pscan_rep_mode;
    u8  pscan_period_mode;
    u8  pscan_mode;
    u8  dev_class[3];
    u16 clock_offset;
} __attribute__((packed)) inquiry_info;

typedef struct
{
    u8  status;
    bdaddr_t bdaddr;
    char name[0];
} __attribute__((packed)) remote_name_info;

typedef struct
{
    u8  status;
    u16 handle;
    bdaddr_t bdaddr;
    u8  link_type;
    u8  encr_mode;
} __attribute__((packed)) connection_info;

typedef struct {
    bdaddr_t bdaddr;
    u8     dev_class[3];
    u8     link_type;
} __attribute__((packed)) connection_request_info;

typedef struct {
    bdaddr_t bdaddr;
    int socket;
    int state;
} __attribute__((packed)) socket_state;

typedef struct {
    bdaddr_t bdaddr;
    u8 status;
} __attribute__((packed)) authentication_complete_info;

typedef struct {
    union {
        bdaddr_t bdaddr;
        inquiry_info ii;
        remote_name_info ri;
        connection_info ci;
        connection_request_info cri;
        authentication_complete_info aci;
        socket_state ss;
    };
} cbdata;

//==================================================================
//==================================================================

enum {
    L2CAP_NONE = 0,
    L2CAP_LISTENING = 1,
    L2CAP_OPENING = 2,
    L2CAP_OPEN = 3,
    L2CAP_CLOSED = 4,
    L2CAP_DELETED = 5
};

enum HCI_CALLBACK_EVENT
{
    CALLBACK_NONE,
    CALLBACK_READY,
    CALLBACK_INQUIRY_RESULT,
    CALLBACK_INQUIRY_DONE,
    CALLBACK_REMOTE_NAME,
    CALLBACK_CONNECTION_REQUEST,       // inbound connection
    CALLBACK_DISCONNECTION_COMP,
    CALLBACK_CONNECTION_COMPLETE,
    CALLBACK_AUTHENTICATION_COMPLETE,
    CALLBACK_CONNECTION_FAILED,
    CALLBACK_L2CAP_SOCKET_STATE_CHANGED
};

const char* batostr(const bdaddr_t& a);
bdaddr_t strtoba(const char *str);

// hci
typedef int (*hci_callback)(HCI_CALLBACK_EVENT evt, const void* data, int len, void *ref);
int hci_init(hci_callback cb, void* ref);
int hci_start_inquiry(int secs);
int hci_connect(const bdaddr_t* addr);
int hci_accept_connection_request(const bdaddr_t* addr);
int hci_authentication_requested(const bdaddr_t* addr);
int hci_remote_name_request(const bdaddr_t* addr);
int hci_update();

// l2cap
int l2_open(const bdaddr_t* addr, int psm, bool listen = false);
int l2_send(int s, const uint8_t* data, int len);
int l2_recv(int s, uint8_t* data, int len);
int l2_close(int s);
int l2_state(int s);

// store state, show messages
int sys_get_pref(const char* key, char* value, int max_len);
void sys_set_pref(const char* key, const char* value);
void sys_msg(const char* msg);          // temporarily display a msg

int read_link_key(const bdaddr_t* addr, uint8_t* key);  // do we have a link key for this device?

#endif
