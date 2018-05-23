#ifndef __COIN_CONF_H__
#define __COIN_CONF_H__

#include <libex/list.h>
#include <libex/file.h>
#include <libex/json.h>

#define COIN_SUCCESS 0
#define COIN_NOJSON 100
#define COIN_INVJSON 101
#define COIN_NOMETHOD 102
#define COIN_CONF_NOT_FOUND 103

#define ETHER_DIGITS 18
#define BTC_DIGITS 8
#define LTC_DIGITS 8
#define MCH_MAX_DIGITS 8

#define DEFAULT_PROT "http"
#define DEFAULT_HOST "localhost"
#define DEFAULT_BTC_PORT 8332
#define DEFAULT_LTC_PORT 9432
#define DEFAULT_ETH_PORT 8545

#define ETH_NULL "0x0"

extern __thread int coin_errcode;
extern __thread char coin_errmsg [256];

typedef list_t mch_conf_t;

typedef struct {
    str_t *name;
    str_t *chain_protocol;
    int rpc_port;
    int network_port;
    int target_block_time;
    str_t *rpc_host;
    str_t *rpc_prot;
    str_t *rpc_user;
    str_t *rpc_pass;
    str_t *host;
} chain_conf_t;
extern chain_conf_t btc_conf;
extern chain_conf_t *btc_conf_ptr;
extern chain_conf_t ltc_conf;
extern chain_conf_t *ltc_conf_ptr;
extern chain_conf_t eth_conf;
extern mch_conf_t *mch_conf;

int btc_load_conf (const char *home);
int ltc_load_conf (const char *home);
void mch_load_conf (const char *home);
void eth_set_conf (const char *rpc_prot, size_t rpc_prot_len, const char *rpc_host, size_t rpc_host_len, int rpc_port);

chain_conf_t *mch_get_conf (const char *name, size_t name_len);

void coin_update_host (chain_conf_t *conf);

str_t *str_to_int_str (const char *ether_amount, int scale);

#endif // __COIN_CONF_H__
