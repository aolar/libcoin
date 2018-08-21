#ifndef __COIN_H__
#define __COIN_H__

#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <gmp.h>
#include <curl/curl.h>
#include <libex/str.h>
#include <libex/list.h>
#include <libex/json.h>
#include "conf.h"

#define COIN_DSCALE 8

typedef enum { TX_SEND, TX_RECV } tx_category_t;

static inline str_t *coin_str_to_int_str (const char *amount) {
    return str_to_int_str(amount, COIN_DSCALE);
}

char *coin_double_to_int_str (double amount);

double coin_getbalance (chain_conf_t *conf, const char *account);
char *coin_getbalance_as_str (chain_conf_t *conf, const char *account);
char *coin_getnewaddress (chain_conf_t *conf, const char *account);
int coin_listtransactions (chain_conf_t *conf, const char *account, int from, int count, json_item_h fn, void *userdata, int flags);
int coin_listaccounts (chain_conf_t *conf, json_item_h fn, void *userdata, int flags);
int coin_getaddressesbyaccount (chain_conf_t *conf, const char *account, json_item_h fn, void *userdata, int flags);
int coin_gettransaction (chain_conf_t *conf, const char *txid, json_item_h fn, void *userdata);
char *coin_sendfrom_z (chain_conf_t *conf, const char *from_account, const char *to_address, mpz_srcptr amount);
char *coin_sendtoaddress_z (chain_conf_t *conf, const char *to_address, mpz_srcptr amount);
char *coin_encryptwallet (chain_conf_t *conf, const char *pass);
void coin_walletlock (chain_conf_t *conf);
void coin_walletpassphrase (chain_conf_t *conf, const char *pass, int timeout);
void coin_stop (chain_conf_t *conf);

// -= BTC =-
static inline double btc_getbalance (const char *account) {
    return coin_getbalance(btc_conf_ptr, account);
}

static inline char *btc_getbalance_as_str (const char *account) {
    return coin_getbalance_as_str(btc_conf_ptr, account);
}

static inline char *btc_getnewaddress (const char *account) {
    return coin_getnewaddress(btc_conf_ptr, account);
}

static inline int btc_listtransactions (const char *account, int from, int count, json_item_h fn, void *userdata, int flags) {
    return coin_listtransactions(btc_conf_ptr, account, from, count, fn, userdata, flags);
}

static inline int btc_listaccounts (json_item_h fn, void *userdata, int flags) {
    return coin_listaccounts(btc_conf_ptr, fn, userdata, flags);
}

static inline int btc_getaddressesbyaccount (const char *account, json_item_h fn, void *userdata, int flags) {
    return coin_getaddressesbyaccount(btc_conf_ptr, account, fn, userdata, flags);
}

static inline int btc_gettransaction (const char *txid, json_item_h fn, void *userdata) {
    return coin_gettransaction(btc_conf_ptr, txid, fn, userdata);
}

static inline char *btc_sendfrom_z (const char *from_account, const char *to_address, mpz_srcptr amount) {
    return coin_sendfrom_z(btc_conf_ptr, from_account, to_address, amount);
}

static inline char *btc_sendtoaddress_z (const char *to_address, mpz_srcptr amount) {
    return coin_sendtoaddress_z(btc_conf_ptr, to_address, amount);
}

static inline char *btc_encryptwallet (const char *pass) {
    return coin_encryptwallet(btc_conf_ptr, pass);
}

static inline void btc_walletlock () {
    coin_walletlock(btc_conf_ptr);
}

static inline void btc_walletpassphrase (const char *pass, int timeout) {
    coin_walletpassphrase(btc_conf_ptr, pass, timeout);
}

static inline void btc_stop () {
    coin_stop(btc_conf_ptr);
}

// -= LTC =-
static inline double ltc_getbalance (const char *account) {
    return coin_getbalance(ltc_conf_ptr, account);
}

static inline char *ltc_getbalance_as_str (const char *account) {
    return coin_getbalance_as_str(ltc_conf_ptr, account);
}

static inline char *ltc_getnewaddress (const char *account) {
    return coin_getnewaddress(ltc_conf_ptr, account);
}

static inline int ltc_listtransactions (const char *account, int from, int count, json_item_h fn, void *userdata, int flags) {
    return coin_listtransactions(ltc_conf_ptr, account, from, count, fn, userdata, flags);
}

static inline int ltc_listaccounts (json_item_h fn, void *userdata, int flags) {
    return coin_listaccounts(ltc_conf_ptr, fn, userdata, flags);
}

static inline int ltc_getaddressesbyaccount (const char *account, json_item_h fn, void *userdata, int flags) {
    return coin_getaddressesbyaccount(ltc_conf_ptr, account, fn, userdata, flags);
}

static inline int ltc_gettransaction (const char *txid, json_item_h fn, void *userdata) {
    return coin_gettransaction(ltc_conf_ptr, txid, fn, userdata);
}

static inline char *ltc_sendfrom_z (const char *from_account, const char *to_address, mpz_srcptr amount) {
    return coin_sendfrom_z(ltc_conf_ptr, from_account, to_address, amount);
}

static inline char *ltc_sendtoaddress_z (const char *to_address, mpz_srcptr amount) {
    return coin_sendtoaddress_z(ltc_conf_ptr, to_address, amount);
}

static inline char *ltc_encryptwallet (const char *pass) {
    return coin_encryptwallet(ltc_conf_ptr, pass);
}

static inline void ltc_walletlock () {
    coin_walletlock(ltc_conf_ptr);
}

static inline void ltc_walletpassphrase (const char *pass, int timeout) {
    coin_walletpassphrase(ltc_conf_ptr, pass, timeout);
}

static inline void ltc_stop () {
    coin_stop(ltc_conf_ptr);
}

// Multichain
#define MCH_STREAM_PUBKEYS 0x01
#define MCH_STREAM_ITEMS 0x02
#define MCH_STREAM_ACCESS 0x04
typedef int mch_stream_t;

#define MCA_ISCOMPRESSED 0x00000001
#define MCA_ISMINE 0x00000002
#define MCA_ISSCRIPT 0x00000004
#define MCA_ISVALID 0x00000008
#define MCA_ISWATCHONLY 0x00000010
#define MCA_ISSYCHRONIZED 0x00000020
typedef struct {
    char *account;
    char *address;
    int flags;
    char *pubkey;
} mch_address_t;
void mch_address_free (mch_address_t *addr);

typedef void (*balance_h) (strptr_t*, strptr_t*, long double, void*);
int mch_getmultibalances (chain_conf_t *conf,
                          const char *addresses, size_t addresses_len,
                          const char *assets, size_t assets_len,
                          int miniconf, balance_h fn, void *userdata);
char *mch_getnewaddress (chain_conf_t *conf);
int mch_getaddressbalances (chain_conf_t *conf,
                            const char *address, size_t address_len,
                            int miniconf, json_item_h fn, void *userdata, int flags);
char *mch_issue (chain_conf_t *conf,
                 const char *asset, size_t asset_len,
                 const char *address, size_t address_len,
                 double amount, int digits, int can_issuemore);
char *mch_sendfrom_d (chain_conf_t *conf,
                      const char *from_address, size_t from_address_len,
                      const char *to_address, size_t to_address_len,
                      const char *asset, size_t asset_len, double amount);
mch_address_t *mch_validateaddress (chain_conf_t *conf, const char *address, size_t address_len);
int mch_listaddresstransactions (chain_conf_t *conf,
                                 const char *address, size_t address_len,
                                 int from, int count, json_item_h fn, void *userdata, int flags);
int mch_getwallettransaction (chain_conf_t *conf,
                              const char *txid, size_t txid_len,
                              json_item_h fn, void *userdata);
int mch_listpermissions (chain_conf_t *conf,
                         const char *address, size_t address_len,
                         json_item_h fn, void *userdata);
int mch_grant (chain_conf_t *conf,
               const char *address, size_t address_len,
               const char *permissions, size_t permissions_len);
int mch_revoke (chain_conf_t *conf,
                const char *address, size_t address_len,
                const char *permissions, size_t permissions_len);
char *mch_dumpprivkey (chain_conf_t *conf,
                       const char *address, size_t address_len);
int mch_importprivkey (chain_conf_t *conf,
                       const char *privkey, size_t privkey_len,
                       int is_rescan);
int mch_getblockchainparams (chain_conf_t *conf, json_item_h fn, void *userdata, int flags);
int mch_getaddresses (chain_conf_t *conf, json_item_h fn, void *userdata, int flags);
int mch_dumpwallet (chain_conf_t *conf, const char *filename);
int mch_importwallet (chain_conf_t *conf, const char *filename);
char *mch_createstream (chain_conf_t *conf, mch_stream_t mch_stream, int is_open);
char *mch_publishform (chain_conf_t *conf, const char *address, size_t address_len,
                       mch_stream_t mch_stream, char *key,
                       char *data_hex, size_t data_hext_len);
int mch_subscribestream (chain_conf_t *conf, mch_stream_t mch_stream);
int mch_liststreampublisheritems (chain_conf_t *conf, mch_stream_t mch_stream,
                                  const char *address, size_t address_len, int from, int count,
                                  json_item_h fn, void *userdata, int flags);
char *mch_gettxoutdata (chain_conf_t *conf, const char *txid, size_t txid_len, int vout);
char *mch_stop (chain_conf_t *conf);

#endif // __COIN_H__
