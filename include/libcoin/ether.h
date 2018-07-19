#ifndef __ETHEREUM_H__
#define __ETHEREUM_H__

#include <stdarg.h>
#include <stdint.h>
#include <gmp.h>
#include <curl/curl.h>
#include <libex/str.h>
#include <libex/json.h>
#include <libex/array.h>
#include "conf.h"

#define ETH_DSCALE 18
#define ETH_UNLOCK_TIMEOUT 1500

extern mpz_t eth_gwei;
extern mpz_t eth_fast_price;
extern mpz_t eth_average_price;
extern mpz_t eth_slow_price;

static inline str_t *ether_str_to_int_str (const char *ether_amount) {
    return str_to_int_str(ether_amount, ETH_DSCALE);
}

char *ether_hex_to_int_str (const char *amount);

typedef enum { ST_NONE, ST_NONPAYABLE, ST_VIEW } eth_method_state_t;
typedef struct {
    char *proto;
    char *sign;
    eth_method_state_t state;
    int8_t is_payable;
    char **outputs;
    size_t output_len;
} eth_method_t;

DEFINE_SORTED_ARRAY(eth_method_list_t, eth_method_t);

typedef struct {
    char *address;
    eth_method_list_t *methods;
} eth_contract_t;

typedef enum { ETH_CALL, ETH_SENDTRAN } eth_call_type_t;

typedef struct {
    eth_contract_t *contract;
    eth_method_t *method;
    strbuf_t buf;
    json_t *json;
    eth_call_type_t call_type;
    str_t *result;
} eth_ctx_t;
void eth_ctx_clear (eth_ctx_t *ctx);

eth_contract_t *eth_declare_contract (const char *address);
eth_contract_t *eth_load_contract (const char *address, const char *intf, size_t intf_len);
eth_method_t *eth_declare_method (eth_contract_t *contract, const char *proto, size_t proto_len);
void eth_free_contract (eth_contract_t *contract);

int eth_unlock_account (const char *account, const char *passwd, int timeout);
char *eth_get_coinbase (char *address, size_t address_len);
char *eth_coinbase ();
char *eth_new_account (const char *pass, char *address, size_t address_len);
str_t *eth_sha3 (const char *param, size_t param_len);

int eth_prepare_exec (eth_contract_t *contract, const char *from, const char *proto, eth_ctx_t *ctx);
void eth_param_address (eth_ctx_t *ctx, const char *address);
void eth_param_int (eth_ctx_t *ctx, mpz_t x);
int eth_exec (eth_ctx_t *ctx, const char *value, const char *gas, const char *gas_price);

void eth_rawstr_to_wei (const char *rawstr, mpz_t amount);
void eth_rawstr_to_ether (const char *rawstr, mpf_t amount);

char *eth_send (const char *from, const char *to, const char *amount, const char *gas, const char *gas_price);
char *eth_send_wei (const char *from, const char *to, mpz_srcptr amount, mpz_srcptr gas, mpz_srcptr gas_price);
char *eth_send_ether (const char *from, const char *to, mpq_srcptr amount, mpz_srcptr gas, mpz_srcptr gas_price);
char *eth_newaccount (const char *passwd);
char *eth_getbalance (const char *address, size_t address_len);
int eth_accounts (json_item_h fn, void *userdata, int flags);
int eth_gettransaction_by_hash (const char *hash, size_t hash_len, json_item_h fn, void *userdata);

void eth_mine ();

#endif // __ETHEREUM_H__
