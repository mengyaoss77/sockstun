/*
*   local.h
*   Author: mengyaoss77
*/

#define VER_SOCKS5  0x05

#define CMD_CONNECT 0x01
#define CMD_BIND    0x02
#define CMD_UDP     0x03

#define REP_SUCCESSS    0x00
#define REP_CONNECTFAIL 0x01
#define REP_REJECT      0x02
#define REP_UNREACHABLE 0x03
#define REP_UNKNOWNHOST 0x04
#define REP_REJECTED    0x05
#define REP_TIMEOUT     0x06
#define REP_ERRCMD      0x07
#define REP_ERRADDR     0x08

#define RSV         0x00

#define ATYP_4      0X01
#define ATYP_NAME   0x03
#define ATYP_6      0x04

void forward(int, int);

struct socksrequest {
    uint8_t     s_ver;
    uint8_t     s_cmd;
    uint8_t     s_rsv;
    uint8_t     s_atyp;
    char *      addr;
    uint16_t    s_port;
};

struct socksresponse {
    uint8_t     s_ver;
    uint8_t     s_rep;
    uint8_t     s_rsv;
    uint8_t     s_atyp;
    char        addr[4];
    uint16_t    s_port;
};