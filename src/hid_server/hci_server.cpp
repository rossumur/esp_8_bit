
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <string>
#include <map>
#include <vector>
#include <mutex>
using namespace std;

#include "hci_transport.h"
#include "hci_server.h"

#if 0
#define TRACE trace
#define PRINTF printf
#else
#define TRACE
#define PRINTF
#endif

#define HCI_GRP_LINK_CONT_CMDS             (0x01 << 10) // 0x0400
#define HCI_GRP_LINK_POLICY_CMDS           (0x02 << 10) // 0x0800
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10) // 0x0C00
#define HCI_GRP_INFO_PARAMS_CMDS           (0x04 << 10) // 0x1000

#define HCI_READ_BUFFER_SIZE               (0x0005 | HCI_GRP_INFO_PARAMS_CMDS)
#define HCI_READ_BD_ADDR                   (0x0009 | HCI_GRP_INFO_PARAMS_CMDS)

#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_READ_STORED_LINK_KEY           (0x000D | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_STORED_LINK_KEY          (0x0011 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_LOCAL_NAME               (0x0013 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_CLASS_OF_DEVICE          (0x0024 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_SCAN_ENABLE              (0x001A | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_AUTH_ENABLE              (0x0020 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)

#define HCI_ROLE_DISCOVERY                 (0x0009 | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_SWITCH_ROLE                    (0x000B | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_WRITE_LINK_POLICY              (0x000D | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_READ_DEFAULT_LINK_POLICY       (0x000E | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_WRITE_DEFAULT_LINK_POLICY      (0x000F | HCI_GRP_LINK_POLICY_CMDS)

#define HCI_INQUIRY                        (0x0001 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_INQUIRY_CANCEL                 (0x0002 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_CREATE_CONNECTION              (0x0005 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_DISCONNECT                     (0x0006 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_ACCEPT_CONNECTION_REQUEST      (0x0009 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_REJECT_CONNECTION_REQUEST      (0x000A | HCI_GRP_LINK_CONT_CMDS)
#define HCI_LINK_KEY_REQUEST_REPLY         (0x000B | HCI_GRP_LINK_CONT_CMDS)
#define HCI_LINK_KEY_REQUEST_NEG_REPLY     (0x000C | HCI_GRP_LINK_CONT_CMDS)
#define HCI_PIN_CODE_REQUEST_REPLY         (0x000D | HCI_GRP_LINK_CONT_CMDS)
#define HCI_PIN_CODE_REQUEST_NEG_REPLY     (0x000E | HCI_GRP_LINK_CONT_CMDS)
#define HCI_AUTHENTICATION_REQUESTED       (0x0011 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_REMOTE_NAME_REQUEST            (0x0019 | HCI_GRP_LINK_CONT_CMDS)


#define HCI_INQUIRY_COMP_EVT                0x01
#define HCI_INQUIRY_RESULT_EVT              0x02
#define HCI_CONNECTION_COMP_EVT             0x03
#define HCI_CONNECTION_REQUEST_EVT          0x04
#define HCI_DISCONNECTION_COMP_EVT          0x05
#define HCI_AUTHENTICATION_COMP_EVT         0x06
#define HCI_RMT_NAME_REQUEST_COMP_EVT       0x07
#define HCI_ENCRYPTION_CHANGE_EVT           0x08
#define HCI_CHANGE_CONN_LINK_KEY_EVT        0x09
#define HCI_MASTER_LINK_KEY_COMP_EVT        0x0A
#define HCI_READ_RMT_FEATURES_COMP_EVT      0x0B
#define HCI_READ_RMT_VERSION_COMP_EVT       0x0C
#define HCI_QOS_SETUP_COMP_EVT              0x0D
#define HCI_COMMAND_COMPLETE_EVT            0x0E
#define HCI_COMMAND_STATUS_EVT              0x0F

#define HCI_HARDWARE_ERROR_EVT              0x10
#define HCI_FLUSH_OCCURED_EVT               0x11
#define HCI_ROLE_CHANGE_EVT                 0x12
#define HCI_NUM_COMPL_DATA_PKTS_EVT         0x13
#define HCI_MODE_CHANGE_EVT                 0x14
#define HCI_RETURN_LINK_KEYS_EVT            0x15
#define HCI_PIN_CODE_REQUEST_EVT            0x16
#define HCI_LINK_KEY_REQUEST_EVT            0x17
#define HCI_LINK_KEY_NOTIFICATION_EVT       0x18
#define HCI_LOOPBACK_COMMAND_EVT            0x19
#define HCI_DATA_BUF_OVERFLOW_EVT           0x1A
#define HCI_MAX_SLOTS_CHANGED_EVT           0x1B
#define HCI_READ_CLOCK_OFF_COMP_EVT         0x1C
#define HCI_CONN_PKT_TYPE_CHANGE_EVT        0x1D

#define HCI_ROLE_MASTER     0x00
#define HCI_ROLE_SLAVE      0x01

const char* hci_cmd(int c)
{
    switch (c) {
        case HCI_READ_BUFFER_SIZE:  return "HCI_READ_BUFFER_SIZE";
        case HCI_READ_BD_ADDR:      return "HCI_READ_BD_ADDR";

        case HCI_RESET:                 return "HCI_RESET";
        case HCI_READ_STORED_LINK_KEY:  return "HCI_READ_STORED_LINK_KEY";
        case HCI_WRITE_STORED_LINK_KEY: return "HCI_WRITE_STORED_LINK_KEY";
        case HCI_WRITE_LOCAL_NAME:      return "HCI_WRITE_LOCAL_NAME";
        case HCI_WRITE_CLASS_OF_DEVICE: return "HCI_WRITE_CLASS_OF_DEVICE";
        case HCI_WRITE_SCAN_ENABLE:     return "HCI_WRITE_SCAN_ENABLE";
        case HCI_WRITE_AUTH_ENABLE:     return "HCI_WRITE_AUTH_ENABLE";

        case HCI_ROLE_DISCOVERY:        return "HCI_ROLE_DISCOVERY";
        case HCI_SWITCH_ROLE:           return "HCI_SWITCH_ROLE";
        case HCI_WRITE_LINK_POLICY:     return "HCI_WRITE_LINK_POLICY";

        case HCI_INQUIRY:               return "HCI_INQUIRY";
        case HCI_INQUIRY_CANCEL:        return "HCI_INQUIRY_CANCEL";
        case HCI_CREATE_CONNECTION:     return "HCI_CREATE_CONNECTION";
        case HCI_DISCONNECT:            return "HCI_DISCONNECT";
        case HCI_ACCEPT_CONNECTION_REQUEST:     return "HCI_ACCEPT_CONNECTION_REQUEST";
        case HCI_REJECT_CONNECTION_REQUEST:     return "HCI_REJECT_CONNECTION_REQUEST";
        case HCI_LINK_KEY_REQUEST_REPLY:        return "HCI_LINK_KEY_REQUEST_REPLY";
        case HCI_LINK_KEY_REQUEST_NEG_REPLY:    return "HCI_LINK_KEY_REQUEST_NEG_REPLY";
        case HCI_PIN_CODE_REQUEST_REPLY:        return "HCI_PIN_CODE_REQUEST_REPLY";
        case HCI_PIN_CODE_REQUEST_NEG_REPLY:    return "HCI_PIN_CODE_REQUEST_NEG_REPLY";
        case HCI_AUTHENTICATION_REQUESTED:      return "HCI_AUTHENTICATION_REQUESTED";
        case HCI_REMOTE_NAME_REQUEST:           return "HCI_REMOTE_NAME_REQUEST";
    }
    static char s[32];
    sprintf(s,"HCICMD:%04X",c);
    return s;
}

const char* _hci_evt[] = {
    "HCI_0_EVT", //                0x00
    "HCI_INQUIRY_COMP_EVT", //                0x01
    "HCI_INQUIRY_RESULT_EVT", //              0x02
    "HCI_CONNECTION_COMP_EVT", //             0x03
    "HCI_CONNECTION_REQUEST_EVT", //          0x04
    "HCI_DISCONNECTION_COMP_EVT", //          0x05
    "HCI_AUTHENTICATION_COMP_EVT", //         0x06
    "HCI_RMT_NAME_REQUEST_COMP_EVT", //       0x07
    "HCI_ENCRYPTION_CHANGE_EVT", //           0x08
    "HCI_CHANGE_CONN_LINK_KEY_EVT", //        0x09
    "HCI_MASTER_LINK_KEY_COMP_EVT", //        0x0A
    "HCI_READ_RMT_FEATURES_COMP_EVT", //      0x0B
    "HCI_READ_RMT_VERSION_COMP_EVT", //       0x0C
    "HCI_QOS_SETUP_COMP_EVT", //              0x0D
    "HCI_COMMAND_COMPLETE_EVT", //            0x0E
    "HCI_COMMAND_STATUS_EVT", //              0x0F

    "HCI_HARDWARE_ERROR_EVT", //              0x10
    "HCI_FLUSH_OCCURED_EVT", //               0x11
    "HCI_ROLE_CHANGE_EVT", //                 0x12
    "HCI_NUM_COMPL_DATA_PKTS_EVT", //         0x13
    "HCI_MODE_CHANGE_EVT", //                 0x14
    "HCI_RETURN_LINK_KEYS_EVT", //            0x15
    "HCI_PIN_CODE_REQUEST_EVT", //            0x16
    "HCI_LINK_KEY_REQUEST_EVT", //            0x17
    "HCI_LINK_KEY_NOTIFICATION_EVT", //       0x18
    "HCI_LOOPBACK_COMMAND_EVT", //            0x19
    "HCI_DATA_BUF_OVERFLOW_EVT", //           0x1A
    "HCI_MAX_SLOTS_CHANGED_EVT", //           0x1B
    "HCI_READ_CLOCK_OFF_COMP_EVT", //         0x1C
    "HCI_CONN_PKT_TYPE_CHANGE_EVT", //        0x1D
};

const char* hci_evt(int c)
{
    if (c <= 0x1D)
        return _hci_evt[c];
    static char s[32];
    sprintf(s,"HCIEVT:%04X",c);
    return s;
}

typedef struct {
    u16 interval_min; // * 0.625 msec  0x20..0x4000 (<= max)
    u16 interval_max; // * 0.625 msec  0x20..0x4000 (>= min)
    u8 type;
    u8 own_addr_type;
    u8 peer_addr_type;
    u8 peer_addr[6];
    u8 adv_chan_map; // default 7 (all channels)
    u8 filter_policy; // 0=any
} __attribute__((packed)) hci_adv_params;

typedef struct {
    u8 scan_type; // 0: passive 1: active
    u16 scan_interval; // * 0.625 ms
    u16 scan_window; // * 0.625 ms
    u8 own_addr_type; // 0:public 1:random
    u8 filter_policy; // 0:all
} __attribute__((packed)) hci_scan_parameters;

typedef struct {
    u8 status;
    u16 acl_plen;
    u8 sco_plen;
    u16 acl_pkts;
    u16 sco_pkts;
} __attribute__((packed)) hci_buffer_size;

typedef struct {
    u8 status;
    u16 acl_plen;
    u8 acl_pkts;
} __attribute__((packed)) hci_le_buffer_size;

typedef struct {
    u16 handle;
    u8 reason;
} __attribute__((packed)) hci_disconnect;

typedef struct  {
    u8     status;
    u16   handle;
    u8     reason;
} __attribute__((packed)) hci_disconn_complete;

typedef struct  {
    u8     status;
    u16   handle;
} __attribute__((packed)) hci_auth_complete;

typedef struct  {
    u8    status;
    u16   handle;
    u8    role;
} __attribute__((packed)) hci_role_discovery_complete;

typedef struct  {
    u8    status;
    u16   max_keys;
    u16   num_keys;
} __attribute__((packed)) hci_read_stored_link_keys_complete;

typedef struct  {
    u8     status;
    bdaddr_t     peer_addr;
    u8     role;
} __attribute__((packed)) hci_role_change_evt;

struct {
    u16   handle;
    u16   policy;
} __attribute__((packed)) hci_write_link_policy;

#define L2CAP_COMMAND_REJ       0x01
#define L2CAP_CONN_REQ          0x02
#define L2CAP_CONN_RSP          0x03
#define L2CAP_CONF_REQ          0x04
#define L2CAP_CONF_RSP          0x05
#define L2CAP_DISCONN_REQ       0x06
#define L2CAP_DISCONN_RSP       0x07
#define L2CAP_ECHO_REQ          0x08
#define L2CAP_ECHO_RSP          0x09
#define L2CAP_INFO_REQ          0x0a
#define L2CAP_INFO_RSP          0x0b

/* L2CAP command codes */
const char* L2CAP_ComandCodeStr(int c)
{
    switch (c)
    {
        case L2CAP_COMMAND_REJ:    return "L2CAP_COMMAND_REJ";
        case L2CAP_CONN_REQ:    return "L2CAP_CONN_REQ";
        case L2CAP_CONN_RSP:    return "L2CAP_CONN_RSP";
        case L2CAP_CONF_REQ:    return "L2CAP_CONF_REQ";
        case L2CAP_CONF_RSP:    return "L2CAP_CONF_RSP";
        case L2CAP_DISCONN_REQ:    return "L2CAP_DISCONN_REQ";
        case L2CAP_DISCONN_RSP:    return "L2CAP_DISCONN_RSP";
        case L2CAP_ECHO_REQ:    return "L2CAP_ECHO_REQ";
        case L2CAP_ECHO_RSP:    return "L2CAP_ECHO_RSP";
        case L2CAP_INFO_REQ:    return "L2CAP_INFO_REQ";
        case L2CAP_INFO_RSP:    return "L2CAP_INFO_RSP";
    }
    return "unknown";
}

typedef struct
{
    u8     type;          // acl = 2
    u16    handle;
    u16    length;         // total
    u16    l2capLength;    // length -4
    u16    cid;            // Signaling packet CID = 1
    u8     data[0];
} __attribute__((packed)) l2cap_data;

typedef struct
{
    u8    cmd;            //
    u8    id;
    u16   cmdLength;        // total-8
    u16   params[5];      // Params
} __attribute__((packed)) l2cap_cmd;

bdaddr_t strtoba(const char *a)
{
    bdaddr_t b;
    unsigned int d[6];
    sscanf(a,"%02X:%02X:%02X:%02X:%02X:%02X",d+5,d+4,d+3,d+2,d+1,d+0);
    for (int i = 0; i < 6; i++)
        b.b[i] = d[i];
    return b;
}

const char* batostr(const bdaddr_t& a)
{
    static char buf[32];
    auto d = a.b;
    sprintf(buf,"%02X:%02X:%02X:%02X:%02X:%02X",d[5],d[4],d[3],d[2],d[1],d[0]);
    return buf;
}

class PacketQ
{
    uint32_t _read;
    uint32_t _write;
    enum {QSIZE = 32};
    vector<uint8_t> _data[QSIZE];
    mutex _mutex;
public:
    PacketQ() : _read(0),_write(0) {};

    bool empty()
    {
        return _read == _write;
    }
    bool read(vector<uint8_t>& d)
    {
        lock_guard<mutex> lock(_mutex);
        assert(_read <= _write);
        if (_read == _write)
            return false;
        d = _data[_read++ & (QSIZE-1)];
        return true;
    }

    bool write(const vector<uint8_t>& d)
    {
        lock_guard<mutex> lock(_mutex);
        assert(_read <= _write && (_write - _read <= QSIZE));
        if (_write - _read == QSIZE)
            return false;
        _data[_write++ & (QSIZE-1)] = d;
        return true;
    }

    bool write(const uint8_t* data, int len)
    {
        vector<uint8_t> buf(len);
        memcpy(&buf[0],data,len);
        return write(buf);
    }
};

static void to_hex(char* dst, const uint8_t* src, int len)
{
    while (len--) {
        sprintf(dst,"%02X",*src++);
        dst += 2;
    }
}

static int nybble(char c)
{
    const char* h = "0123456789ABCDEF";
    return (strchr(h,toupper(c)) - h);
}

static void from_hex(uint8_t* dst, const char* src, int len)
{
    while (len--) {
        *dst++ = (nybble(src[0]) << 4) | nybble(src[1]);
        src += 2;
    }
}

int read_link_key(const bdaddr_t* addr, uint8_t* key)
{
    char s[64] = {0};
    char ba[12+1] = {0};
    to_hex(ba,addr->b,6);
    if (sys_get_pref(ba,s,sizeof(s)-1)) {
        from_hex(key,s,16);
        return 0;
    }
    return -1;
}

static void write_link_key(const bdaddr_t* addr, const uint8_t* key)
{
    char buf[64];
    char ba[12+1] = {0};
    to_hex(ba,addr->b,6);
    to_hex(buf,key,16);
    sys_set_pref(ba,buf);
}

class BTDevice;
class L2CAPSocket {
public:
    int _state;
    uint16_t _psm;
    uint16_t _scid;
    uint16_t _dcid;
    PacketQ _q;
    BTDevice* _device;

    L2CAPSocket() : _state(0) {};
    void set_state(int state);
    ~L2CAPSocket() {
        set_state(L2CAP_DELETED);
    }
};

// forward
int hci_write(vector<uint8_t>& buf);

enum {
    SLAVE = 1,
    ACTIVE = 2,
    READING_LINK_KEY = 4,
};

class BTDevice {
public:
    bdaddr_t _addr;
    string _name;
    uint8_t _major;
    uint8_t _minor;
    int _dev_class;  //
    int _handle;
    int _flags;

    // packet reassembly
    vector<uint8_t> _packet;
    int _packet_pos;
    int _packet_cid;

    uint8_t _txid;
    map<int,L2CAPSocket*> _sockets;

    BTDevice() : _packet_pos(0),_txid(1),_flags(SLAVE)
    {
    }

    bool is_wii()
    {
        if (_name.find("Nintendo") == 0)
            return true;
        if (_dev_class == 0x042500 || _dev_class == 0x080500)
            return true;
        return false;
    }

    int l2_open(int scid, int psm, bool listen)
    {
        auto* s = new L2CAPSocket();
        s->_psm = psm;
        s->_dcid = 0;
        s->_scid = scid;
        s->_device = this;
        _sockets[scid] = s;

        if (!listen) {
            connect(s->_psm,s->_scid);
            s->set_state(L2CAP_OPENING);
        } else {
            s->set_state(L2CAP_LISTENING);
        }
        return scid;
    }

    int l2_close(int scid, bool this_end)
    {
        auto s = get_socket(scid);
        if (!s)
            return -1;
        if (this_end && s->_state == L2CAP_OPEN)
            disconnect(s->_scid,s->_dcid);  // tell other end
        _sockets.erase(scid);
        return 0;
    }

    L2CAPSocket* get_socket(int scid)
    {
        auto it = _sockets.find(scid);
        if (it == _sockets.end())
            return NULL;
        return it->second;
    }

    // this will happen before hid knows about it
    void hci_connected(const connection_info* ci)
    {
        PRINTF("%s connected on handle %d\n",batostr(ci->bdaddr),ci->handle);
        _handle = ci->handle;
    }

    void set_role(uint8_t role)
    {
        _flags = role ? (_flags | SLAVE) : (_flags & ~SLAVE);
    }

    void disconnect_all()
    {
        for (auto& p : _sockets)
            delete p.second;
        _sockets.clear();
        _handle = -1;
        _flags = SLAVE;
    }

    // inbound l2cap packet. assemble and forward
    bool data(const l2cap_data* p)
    {
        int pt = p->handle >> 12;
        const uint8_t* data = ((const uint8_t*)p) + 5;
        int len = p->length;
        if (pt == 2) {  // start of a packet
            _packet.resize(p->l2capLength); // length of payload
            _packet_cid = p->cid;
            data += 4;
            len -= 4;                       // skip header
        }
        memcpy(&_packet[_packet_pos],data,len);
        _packet_pos += len;

        // if packet is assembled, queue it.
        if (_packet_pos == (int)_packet.size()) {
            if (_packet_cid == 1)
                control((const l2cap_cmd*)&_packet[0]);
            else {
                auto s = get_socket(_packet_cid);
                if (s) {
                    s->_q.write(&_packet[0],_packet_pos);
                } else {
                    _packet_pos = 0;
                    return false;   // did not have a socket for this data.
                }
            }
            _packet_pos = 0;
        }
        return true;
    }

    // wait until connection is completely open
    #define M(_x) (1 << ((_x) + 8))
    void check_open(L2CAPSocket* s)
    {
        if (!s)
            return;
        int mask = M(L2CAP_CONN_RSP) | M(L2CAP_CONF_REQ) | M(L2CAP_CONF_RSP);
        if ((_flags & M(L2CAP_CONN_REQ)) == 0) {    // outbound connection
            if ((mask & _flags) == mask)
                s->set_state(L2CAP_OPEN);   // connection is open
        }
    }

    void control(const l2cap_cmd* c)
    {
        PRINTF("acl recv:%d %s %d:%d:%d:%d len:%d\n",c->id,L2CAP_ComandCodeStr(c->cmd),
               c->params[0],c->params[1],c->params[2],c->params[3],c->cmdLength);

        _txid++;    // TODO. advance _txid on recev?

        L2CAPSocket* s = 0;
        _flags |= M(c->cmd);  // mark which events we have seen
        switch (c->cmd) {
            case L2CAP_CONN_RSP:
            {
                int status = c->params[2];
                s = get_socket(c->params[1]);
                if (status == 0) {    // good status
                    if (s) {
                        s->_dcid = c->params[0];
                        configure_request(s->_dcid);
                    }
                } else {
                    if (status != 1) {
                        if (s)
                            s->set_state(L2CAP_CLOSED);
                        PRINTF("l2cap connection FAILED: %d\n",status);
                    }
                }
            }
                break;

            case L2CAP_CONF_RSP:
                s = get_socket(c->params[0]);
                check_open(s);
                break;

            case L2CAP_CONF_REQ:
                s = get_socket(c->params[0]);
                if (s) {
                    configure_response(c->id,s->_dcid,c->params[3]);    //
                    check_open(s);
                }
                break;

            case L2CAP_DISCONN_REQ:
            {
                l2_close(c->params[0],false); // scid
                uint16_t p[2] = {c->params[1],c->params[0]}; // dcid, scid
                l2cap(L2CAP_DISCONN_RSP,c->id, p, 2);
            }
                break;

            case L2CAP_DISCONN_RSP:
                ::l2_close(c->params[1]); // scid - already closed
                break;

            // active connection from peripheral
            case L2CAP_CONN_REQ:
            {
                int dcid = c->params[1];
                int psm = c->params[0];
                int rxid = c->id;
                _flags |= ACTIVE;   // inbound connection from peripheral

                for (auto& p : _sockets) {
                    auto* s = p.second;
                    if (s->_psm == psm) {
                        u16 p[4];
                        p[0] = s->_scid;       // this end
                        p[1] = dcid;           // other end
                        p[2] = 0;              // ok
                        p[3] = 0;              // no further information
                        l2cap(L2CAP_CONN_RSP,rxid,p,4);
                        s->set_state(L2CAP_OPENING);
                        s->_dcid = dcid;

                        _txid = rxid+1;                 // inbound
                        configure_request(s->_dcid);    // ask to configure
                    }
                }
            }

                break;

            case L2CAP_INFO_REQ:
            {
                u16 p[2] = {c->params[0],1};  // no info
                l2cap(L2CAP_INFO_RSP,c->id,p,2);
            }
                break;

            default:
                PRINTF("%02X l2cap weird %d %s\n",c->cmd,c->cmdLength,L2CAP_ComandCodeStr(c->cmd));
        }
    }

    // l2cap data
    int send(const void* data = 0, int len = 0, int cid = 1)
    {
        int n = sizeof(l2cap_data) + len;
        vector<uint8_t> buf(n);
        l2cap_data* d = (l2cap_data*)&buf[0];
        d->type = 0x02;                 // acl
        d->handle = _handle | 0x2000;
        d->length = len + 4;            // includes l2cap header
        d->l2capLength = len;
        d->cid = cid;
        if (data)
            memcpy(d->data,data,len);   // l2cap payload
        return hci_write(buf);          // send to outbound q.
    }

    int l2cap(uint8_t cmd, uint8_t id, u16* params, int count)
    {
        PRINTF("acl send:%d %s\n",id,L2CAP_ComandCodeStr(cmd));
        l2cap_cmd b;
        b.cmd = cmd;
        b.id = id;
        b.cmdLength = count*2;
        for (int i = 0; i < count; i++)
            b.params[i] = params[i];
        return send(&b,4 + count*2);
    }

    int connect(u16 psm, u16 scid)
    {
        u16 p[2] = { psm, scid };
        return l2cap(L2CAP_CONN_REQ,_txid++,p,2);
    }

    int disconnect(u16 scid, u16 dcid)
    {
        u16 p[2] = { dcid, scid };
        return l2cap(L2CAP_DISCONN_REQ,_txid++,p,2);
    }

    int configure_request(u16 dcid)
    {
        u16 p[4] = { dcid, 0, 0x0201, 0xFFFF }; // Options,672
        return l2cap(L2CAP_CONF_REQ,_txid++,p,4);
    }

    int configure_response(u16 rxid, u16 dcid, u16 mtu)
    {
        u16 p[5] = { dcid, 0, 0, 0x0201, mtu };
        return l2cap(L2CAP_CONF_RSP,rxid,p,5);
    }
};
vector<BTDevice*> _devices;

// track socket state changes
void hci_socket_change(const bdaddr_t& addr, int scid, int state);
void L2CAPSocket::set_state(int state)
{
  _state = state;
  hci_socket_change(_device->_addr,_scid,state);
}

BTDevice* get_device(const bdaddr_t* addr)
{
    for (auto d : _devices)
        if (memcmp(&d->_addr,addr,6) == 0)
            return d;
    return NULL;
}

BTDevice* get_device_by_handle(int handle)
{
    for (auto d : _devices) {
        if (d->_handle == handle)
            return d;
    }
    return NULL;
}

BTDevice* get_device_by_socket(int scid)
{
    for (auto d : _devices) {
        if (d->_sockets.find(scid) != d->_sockets.end())
            return d;
    }
    return NULL;
}

L2CAPSocket* get_socket(int scid)
{
    for (auto d : _devices) {
        auto s = d->get_socket(scid);
        if (s)
            return s;
    }
    return NULL;
}

// eww. inbound hid from vestigial connection. pass it on anyway

class HCI {
    enum StateMask {
        MASK_RESET = 1,
        MASK_READ_BUFFER_SIZE = 2,
        MASK_READ_BD_ADDR = 4,
        MASK_INITED = 8,
        MASK_INQUIRY = 16,
    };

    hci_handle _hci;
    int _state;

    const char* _localname;
    bdaddr_t _localAddr;
    hci_buffer_size _buffer_size;

    int _cid; // connection id
    PacketQ _rx;
    PacketQ _tx;

public:
    hci_callback _callback;
    void* _callback_ref;

    // create a new socked on this psm
    int l2_open(const bdaddr_t* addr, int psm, bool listen)
    {
        BTDevice* d = get_device(addr);
        if (!d)
            return -1;
        return d->l2_open(_cid++,psm,listen);
    }

    int l2_close(int scid, bool this_end = true)
    {
        BTDevice* d = get_device_by_socket(scid);
        if (!d)
            return -1;
        return d->l2_close(scid,this_end);
    }

    // recv from packet queue
    int l2_recv(int scid, uint8_t* dst, int len)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        vector<uint8_t> d;
        if (!s->_q.read(d))
            return 0;
        len = min(len,(int)d.size());
        memcpy(dst,&d[0],len);
        return len;
    }

    int l2_send(int scid, const uint8_t* data, int len)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        if (s->_device->send(data,len,s->_dcid))
            return -1;
        return len; //
    }

    int l2_state(int scid)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        return s->_state;
    }

    // initiate a connection to this device
    int connect(const bdaddr_t* addr)
    {
        auto *d = get_device(addr);
        if (!d)
            return -1;
        return create_connection(*d);
    }

    HCI(const char* localname) : _localname(localname),_state(-1),_cid(0x40)
    {
        _hci = hci_open();
        if (!_hci)
            PRINTF("hci_open failed\n");
        else {
            hci_set_packet_handler(_hci,packet_,this);
            hci_set_ready_to_send_handler(_hci,ready_to_send_,this);
        }
    }

    ~HCI() {
        hci_close(_hci);
    }

    int init(hci_callback cb, void* ref)
    {
        _callback = cb;
        _callback_ref = ref;
        return 0;
    }

    int start_inquiry(int duration_secs)
    {
        if (_state & MASK_INQUIRY)
            return -1;  // already running
        return inquiry(duration_secs);
    }

    static void trace(int dir, const uint8_t* data, int len)
    {
        static const char *tagnames[5] = { "???", "CMD", "ACL", "ISO", "EVT" };
        fprintf(stdout,"%c %s ",dir ? '>' : '<',tagnames[*data++]);
        len--;
        for (int i = 0; i < len; i++)
            fprintf(stdout,"%02X",data[i]);
        fprintf(stdout,"\n");
    }

    int update()
    {
        // send any pending
        vector<uint8_t> buf;
        while (hci_send_available(_hci) && _tx.read(buf))
        {
            TRACE(1,&buf[0],(int)buf.size());
            hci_send(_hci,&buf[0],(int)buf.size());
        }

        // handle any inbound.
        // hcl/acl ordering challenge. TODO.
        while (_rx.read(buf)) {
            TRACE(0,&buf[0],(int)buf.size());
            switch (buf[0]) {
                case 0x2: acl(&buf[0],(int)buf.size()); break;
                case 0x4: hci(buf[1],&buf[3],buf[2]); break;
                default:
                    PRINTF("bad hci packet\n");
            }
        }
        return 0;
    }

    int write(vector<uint8_t>& buf)
    {
        return _tx.write(buf) ? 0 : -1; // send to outbound q.
    }

private:
    static void packet_(hci_handle h, const uint8_t* d, int len, void* ref)
    {
        ((HCI*)ref)->packet(d,len);
    }

    static void ready_to_send_(hci_handle h, void* ref)
    {
        ((HCI*)ref)->ready_to_send();
    }

    // inbound packet from hci, queue it
    void packet(const uint8_t* data, int len)
    {
        _rx.write(data,len);
    }

    void ready_to_send()
    {
        if (_state == -1) {
            cmd(HCI_RESET);
            _state = 0;
        }
    }

    // hci command
    int cmd(uint16_t c, const void* data = 0, int len = 0)
    {
        vector<uint8_t> buf(len+4);
        if (data)
            memcpy(&buf[4],data,len);
        buf[0] = 0x01;         // hci Command
        buf[1] = (uint8_t)c;
        buf[2] = (uint8_t)(c >> 8);
        buf[3] = len;
        return write(buf); // send to outbound q.
    }

    // look for devices
    int inquiry(uint8_t duration)
    {
        _state |= MASK_INQUIRY;
        uint8_t count = 0;
        uint8_t buf[5] = { 0x33, 0x8B, 0x9E, duration, count};
        return cmd(HCI_INQUIRY,buf,sizeof(buf));
    }

    int inquiry_cancel()
    {
        if (!(_state & MASK_INQUIRY))
            return 0;
        _state &= MASK_INQUIRY;
        return cmd(HCI_INQUIRY_CANCEL,0,0);
    }

    int add_device(const bdaddr_t& addr, const uint8_t* dev_class)
    {
        auto* d = get_device(&addr);
        if (!d) {
            d = new BTDevice();
            d->_name = batostr(addr);
            d->_addr = addr;
            d->_major = dev_class[1] & 0x1f;
            d->_minor = dev_class[0] >> 2;
            d->_dev_class = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
            d->_handle = -1;        // not connected yet
            _devices.push_back(d);
        }
        return 0;
    }

    int inquiry_result(const inquiry_info* info)
    {
        return add_device(info->bdaddr,info->dev_class);
    }

public:
    int remote_name_request(const bdaddr_t& addr)
    {
        u8 buf[6+4] = {0};
        memcpy(buf,addr.b,6);
        buf[7] = 1;
        return cmd(HCI_REMOTE_NAME_REQUEST,buf,sizeof(buf));
    }

    int authentication_requested(const bdaddr_t& addr)
    {
        auto* d = get_device(&addr);
        if (d && d->_handle != -1)
            return cmd(HCI_AUTHENTICATION_REQUESTED,&d->_handle,2);
        return -1;
    }

    int accept_connection_request(const bdaddr_t& addr)
    {
        uint8_t buf[7] = {0};
        memcpy(buf,&addr,6);
        buf[6] = 0;
        return cmd(HCI_ACCEPT_CONNECTION_REQUEST,buf,sizeof(buf));
    }
private:
    BTDevice* remote_name_response(const remote_name_info* ri)
    {
        auto* d = get_device(&ri->bdaddr);
        if (d) {
            d->_name = ri->name;
            PRINTF("remote_name_response %s %s\n",batostr(ri->bdaddr),ri->name);
        }
        return d;
    }

    int create_connection(BTDevice& d)
    {
        if (d._handle != -1)
            return 0;
        d._flags = 0;
        d._handle = 0;  // connection in progress

        u8 buf[6+7] = {0};
        memcpy(buf,d._addr.b,6);
        buf[6] = 0x18;  // DM1, DH1
        buf[7] = 0xCC;  // DM3, DH3, DM5, DH5
        buf[8] = 1;     // Page Repetition R1
        return cmd(HCI_CREATE_CONNECTION,buf,sizeof(buf));
    }
    
    int switch_role(const bdaddr_t& addr, uint8_t role)
    {
        uint8_t buf[6+1];
        memcpy(buf,addr.b,6);
        buf[6] = role;
        return cmd(HCI_SWITCH_ROLE,buf,sizeof(buf));
    }


    // add device to list on inbound connections
    // should only allow connections to stuff we want?
    int connection_request(const connection_request_info* cr)
    {
        add_device(cr->bdaddr,cr->dev_class);   // why do we have devices on both sides?
        auto* d = get_device(&cr->bdaddr);
        d->_flags |= SLAVE; // assume slave
        return 0;
    }

    const char* _hci_errors[41] = {
        "No Error",
        "Unknown HCI Command",
        "No Connection",
        "Hardware Failure",
        "Page Timeout",
        "Authentication Failure",
        "Key Missing",
        "Memory Full",

        "Connection Timeout",
        "Max Number Of Connections",
        "Max Number Of SCO Connections To A Device",
        "ACL Connection Already Exists",
        "Command Disallowed",
        "Host Rejected Due To Limited Resources",
        "Host Rejected Due To Security Reasons",
        "Host Rejected Due To A Remote Device Only A Personal Device",

        "Host Timeout"
        "Unsupported Feature Or Parameter Value",
        "Other End Terminated Connection: User Ended Connection",
        "Other End Terminated Connection: Low Resources",
        "Other End Terminated Connection: About To Power Off",
        "Connection Terminated By Local Host",
        "Repeated Attempts",
        "Pairing Not Allowed",

        "Unknown LMP PDU",
        "Unsupported Remote Feature",
        "SCO Offset Rejected",
        "SCO Interval Rejected",
        "SCO Air Mode Rejected",
        "Invalid LMP Parameters",
        "Unspecified Error",
        "Unsupported LMP Parameter",

        "Role Change Not Allowed",
        "LMP Response Timeout",
        "LMP Error Transaction Collision",
        "LMP PDU Not Allowed",
        "Encryption Mode Not Acceptable",
        "Unit Key Used",
        "QoS Not Supported",
        "Instant Passed",

        "Pairing With Unit Key Not Supported",
    };

    const char* hci_status_str(int e)
    {
        if (e >= 0 && e < 42)
            return _hci_errors[e];
        return "_hci_status_unrecognized";
    }

    void connection_complete(const connection_info* c)
    {
        auto* d = get_device(&c->bdaddr);
        if (d) {
            if (c->status == 0) {
                d->hci_connected(c);
                _callback(CALLBACK_CONNECTION_COMPLETE,c,sizeof(connection_info),_callback_ref);
                d->set_role(HCI_ROLE_MASTER);   // connected as master

                //uint16_t buf2[2] = {(uint16_t)d->_handle,0x0001};   // role switch
                //cmd(HCI_WRITE_LINK_POLICY,buf2,sizeof(buf2));       // use this to boostrap inbound connections

                //uint16_t buf[1] = {(uint16_t)d->_handle};
                //cmd(HCI_AUTHENTICATION_REQUESTED,buf,sizeof(buf));  // ask for auth
            } else {
                PRINTF("hci connection failed: %s\n",hci_status_str(c->status));
                d->_handle = -1;
            }
        }
    }

    // pressing the "sync" button on the back of the wiimote, then the PIN is the bluetooth address of the host backwards.
    int pincode_reply(const uint8_t* data)
    {
        auto* d = get_device((bdaddr_t*)data);
        uint8_t buf[6+1+16] = {0};
        memcpy(buf,data,6);
        if (d->is_wii()) {
            buf[6] = 6;
            //memcpy(buf+7,data,6);           // 1+2 buttons pressed
            memcpy(buf+7,_localAddr.b,6);   // sync button pressed
        } else {
            buf[6] = 4;
            buf[7] = '0';
            buf[8] = '0';
            buf[9] = '0';
            buf[10] = '0';
            sys_msg("enter '0000' on keyboard to pair");
        }
        return cmd(HCI_PIN_CODE_REQUEST_REPLY,buf,sizeof(buf));
    }

    int disconnect(int handle)
    {
        uint8_t buf[3] = { (uint8_t)handle, (uint8_t)(handle >> 8), 1 };
        return cmd(HCI_DISCONNECT,buf,sizeof(buf));
    }

    void hci(uint8_t evt, const uint8_t* data, uint8_t len)
    {
        PRINTF("%s %d bytes\n",hci_evt(evt),len);
        switch (evt) {
            case HCI_INQUIRY_COMP_EVT:
                _state &= ~MASK_INQUIRY;
                _callback(CALLBACK_INQUIRY_DONE,NULL,0,_callback_ref);
                break;

            case HCI_INQUIRY_RESULT_EVT:
            {
                int n = *data++;
                while (n--) {
                    const auto* ii = (const inquiry_info*)data;
                    inquiry_result(ii);
                    _callback(CALLBACK_INQUIRY_RESULT,data,sizeof(inquiry_info),_callback_ref);
                    data += 14;
                }
            }
                break;

            case HCI_CONNECTION_COMP_EVT:
                connection_complete((const connection_info*)data);
                dongle_bug();
                break;

            case HCI_AUTHENTICATION_COMP_EVT:
            {
                auto ac = (const hci_auth_complete*)data;
                auto* d = get_device_by_handle(ac->handle);
                if (d) {
                    authentication_complete_info aci = {d->_addr,data[0]};
                    _callback(CALLBACK_AUTHENTICATION_COMPLETE,&aci,sizeof(aci),_callback_ref);
                }
            }
                break;

            case HCI_CONNECTION_REQUEST_EVT:
            {
                auto* ci = (connection_request_info*)data;
                connection_request(ci);
                _callback(CALLBACK_CONNECTION_REQUEST,data,len,_callback_ref);
            }
                break;

            case HCI_DISCONNECTION_COMP_EVT:
            {
                auto* d = get_device_by_handle(((const hci_disconnect*)(data+1))->handle);
                if (d) {
                    d->disconnect_all();
                    _callback(CALLBACK_DISCONNECTION_COMP,d->_addr.b,6,_callback_ref);
                }
            }
                break;

            case HCI_RMT_NAME_REQUEST_COMP_EVT:
            {
                remote_name_response((const remote_name_info*)data);
                _callback(CALLBACK_REMOTE_NAME,data,len,_callback_ref);
                break;
            }

            case HCI_PIN_CODE_REQUEST_EVT:
                pincode_reply(data);
                break;

            case HCI_ROLE_CHANGE_EVT:
            {
                auto* rc = (const hci_role_change_evt*)data;
                auto* d = get_device(&rc->peer_addr);
                if (d)
                    d->set_role(rc->role);
            }
                break;

            // reject link keys, go with pins instead
            case HCI_LINK_KEY_REQUEST_EVT:
            {
                const auto* a = (const bdaddr_t*)data;
                auto* d = get_device(a);
                if (d) {
                    uint8_t buf[6+16];
                    if (read_link_key(a,buf+6) != -1) {
                        memcpy(buf,data,6);
                        cmd(HCI_LINK_KEY_REQUEST_REPLY,buf,6+16);   // got a key!
                    } else {
                        cmd(HCI_LINK_KEY_REQUEST_NEG_REPLY,data,6); // don't got a key!
                    }
                }
            }
                break;

            case HCI_COMMAND_COMPLETE_EVT:
            {
                int c = data[1] | (data[2] << 8);
                int status = data[3];
                PRINTF(" -> %s %04X %s\n",hci_cmd(c),c,hci_status_str(status));
                switch (c) {
                    //  Init phase 0
                    case HCI_RESET:
                        cmd(HCI_READ_BUFFER_SIZE);
                        _state |= MASK_RESET;
                        break;

                    //  Init phase 1
                    case HCI_READ_BUFFER_SIZE:
                        cmd(HCI_READ_BD_ADDR);
                        _buffer_size = *((const hci_buffer_size*)(data+3));
                        _state |= MASK_READ_BUFFER_SIZE;
                        break;

                    //  Init phase 2
                    case HCI_READ_BD_ADDR:
                    {
                        _localAddr = *((bdaddr_t*)(data+4)); // Local Address
                        _state |= MASK_READ_BD_ADDR;

                        //uint8_t d[3] = {0x04, 0x05, 0x00};    // what should we look like?
                        uint8_t d[3] = {0x07, 0x02, 0x0C};      // smartphone?
                        cmd(HCI_WRITE_CLASS_OF_DEVICE,d,3);

                        d[0] = 3;
                        cmd(HCI_WRITE_SCAN_ENABLE,d,1);
                    }
                        break;

                    //  Init phase 3
                    case HCI_WRITE_SCAN_ENABLE:
                    {
                        _state |= MASK_INITED;
                        _callback(CALLBACK_READY,data+3,1,_callback_ref);

                        uint8_t a = 0;                      // disable auth
                        cmd(HCI_WRITE_AUTH_ENABLE,&a,1);    // we will manually bond where appropriate
                    }
                        break;

                    case HCI_WRITE_LINK_POLICY:
                    {
                        int h = data[4] | (data[5] << 8);   // handle
                        cmd(HCI_ROLE_DISCOVERY,&h,2);       // check my role
                    }
                        break;

                    case HCI_ROLE_DISCOVERY:
                    {
                        auto* ri = (const hci_role_discovery_complete*)(data + 3);
                        auto* d = get_device_by_handle(ri->handle);
                        if (d)
                            d->set_role(ri->role);
                    }
                    break;

                /*
                    case HCI_WRITE_LOCAL_NAME:
                    {
                        uint8_t d[3] = {0x04, 0x05, 0x00};
                        cmd(HCI_WRITE_CLASS_OF_DEVICE,d,3);
                    }
                    break;
                */

                    case HCI_READ_STORED_LINK_KEY:
                        break;
                }
            }
                 break;
            case HCI_HARDWARE_ERROR_EVT:
                break;
            case HCI_COMMAND_STATUS_EVT:    // after starting inquiry/connect etc
            {
                int cmd = data[2] + (data[3] << 8);
                PRINTF(" -> %s %04X %s\n",hci_cmd(cmd),cmd,hci_status_str(data[0]));
            }
                break;
            case HCI_QOS_SETUP_COMP_EVT:
                break;
            case HCI_NUM_COMPL_DATA_PKTS_EVT:
                break;
            case HCI_MODE_CHANGE_EVT:
                break;

            case HCI_RETURN_LINK_KEYS_EVT:
                break;

            case HCI_LINK_KEY_NOTIFICATION_EVT:
                write_link_key((const bdaddr_t*)data,data+6);
                break;
            default:
                PRINTF("unhandled hci case\n");
        }
    }

    // USB dongle bug
    vector<uint8_t> _dongle_bug;
    void dongle_bug()
    {
        if (!_dongle_bug.empty()) {
            acl(&_dongle_bug[0],(int)_dongle_bug.size());
            _dongle_bug.clear();
        }
    }

    void acl(const uint8_t* data, int len)
    {
        l2cap_data* p = (l2cap_data*)data;
        int h = p->handle & 0x0FFF;
        for (auto d : _devices) {
            if (h == d->_handle) {
                d->data(p);
                return;
            }
        }
        
        // interesting bug on libusb/simulator where ACL open packet arrives before hci connection complete
        // hci arrives on libusb_fill_interrupt_transfer, acl on libusb_fill_bulk_transfer but acl gets ahead

        PRINTF("ACL PACKET DROPPED! handle %d, %d bytes\n",h,len);
        _dongle_bug.resize(len);
        memcpy(&_dongle_bug[0],data,len);
    }
};
HCI* _hci = 0;

void hci_socket_change(const bdaddr_t& addr, int scid, int state)
{
    socket_state s = { addr, scid, state };
    _hci->_callback(CALLBACK_L2CAP_SOCKET_STATE_CHANGED,&s,sizeof(s),_hci->_callback_ref);
}

//==================================================================
//==================================================================

int l2_open(const bdaddr_t* addr, int psm, bool listen)
{
    return _hci->l2_open(addr,psm,listen);
}

int l2_send(int s, const uint8_t* data, int len)
{
    return _hci->l2_send(s, data, len);
}

int l2_recv(int s, uint8_t* data, int len)
{
    return _hci->l2_recv(s, data, len);
}

int l2_close(int s)
{
    return _hci->l2_close(s);
}

int l2_state(int s)
{
    return _hci->l2_state(s);
}

int hci_start_inquiry(int secs)
{
    return _hci->start_inquiry(secs);
}

int hci_update()
{
    return _hci->update();
}

int hci_init(hci_callback cb, void* ref)
{
    delete _hci;
    _hci = new HCI("esp_hid");
    return _hci->init(cb,ref);
}

int hci_accept_connection_request(const bdaddr_t* addr)
{
    return _hci->accept_connection_request(*addr);
}

int hci_authentication_requested(const bdaddr_t* addr)
{
    return _hci->authentication_requested(*addr);
}

int hci_remote_name_request(const bdaddr_t* addr)
{
    return _hci->remote_name_request(*addr);
}

int hci_connect(const bdaddr_t* addr)
{
    return _hci->connect(addr);
}

int hci_write(vector<uint8_t>& buf)
{
    return _hci->write(buf); // send to outbound q.
}
