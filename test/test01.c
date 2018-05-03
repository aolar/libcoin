#include "coin.h"
#include "ether.h"

static void create_addresses (const char *token, const char *arg2) {
    char *address = NULL;
    if (0 == strcasecmp(token, "btc"))
        address = btc_getnewaddress(arg2);
    else
    if (0 == strcasecmp(token, "ltc"))
        address = ltc_getnewaddress(arg2);
    else
    if (0 == strcasecmp(token, "eth"))
        address = eth_newaccount(arg2);
    if (address) {
        printf("%s: %s\n", token, address);
        free(address);
    }
}

static int on_coin_account (json_item_t *j, void *dummy) {
    if (JSON_DOUBLE == j->type) {
        char *name = strndup(j->key.ptr, j->key.len),
             *amount = coin_double_to_int_str(j->data.d);
        printf("\"%s\": %s\n", name, amount);
        free(amount);
        free(name);
    }
    return ENUM_CONTINUE;
}

static int on_eth_account (json_item_t *j, void *dummy) {
    if (JSON_STRING == j->type) {
        char *str = eth_getbalance(j->data.s.ptr, j->data.s.len),
             *account = strndup(j->data.s.ptr, j->data.s.len),
             *balance = ether_hex_to_int_str(str);
        printf("%s: %s\n", account, balance);
        free(str);
        free(account);
        free(balance);
    }
    return ENUM_CONTINUE;
}

static void list_accounts (const char *token) {
    if (0 == strcasecmp(token, "btc")) {
        printf("BTC:\n");
        btc_listaccounts(on_coin_account, NULL, 0);
    } else
    if (0 == strcasecmp(token, "ltc")) {
        printf("LTC\n");
        ltc_listaccounts(on_coin_account, NULL, 0);
    } else
    if (0 == strcasecmp(token, "eth")) {
        printf("ETH:\n");
        eth_accounts(on_eth_account, NULL, 0);
    }
}

static void send_tokens (const char *token, const char *from, const char *to, const char *amount, const char *pass) {
    if (0 == strcasecmp(token, "btc")) {
        str_t *s_amount = coin_str_to_int_str(amount);
        mpz_t z;
        mpz_init_set_str(z, s_amount->ptr, 10);
        if (0 == strcmp(from, "\"\""))
            from = NULL;
        char *txid = btc_sendfrom_z(from, to, z);
        mpz_clear(z);
        printf("%s\n", txid);
        free(txid);
    } else
    if (0 == strcasecmp(token, "ltc")) {
        str_t *s_amount = coin_str_to_int_str(amount);
        mpz_t z;
        mpz_init_set_str(z, s_amount->ptr, 10);
        if (0 == strcmp(from, "\"\""))
            from = NULL;
        char *txid = ltc_sendfrom_z(from, to, z);
        mpz_clear(z);
        printf("%s\n", txid);
        free(txid);
    } else
    if (0 == strcasecmp(token, "eth") && pass) {
        str_t *s_amount = ether_str_to_int_str(amount);
        mpz_t z, zg;
        mpz_init_set_str(z, s_amount->ptr, 10);
        mpz_init_set_str(zg, "21000", 10);
        if (1 == eth_unlock_account(from, pass, 0)) {
            char *txid = eth_send_wei(from, to, z, zg, eth_fast_price);
            printf("%s\n", txid);
            free(txid);
        }
        mpz_clear(zg);
        mpz_clear(z);
        free(s_amount);
    }
}

static char *encryptwallet (const char *token, const char *pass) {
    char *res = NULL;
    if (0 == strcasecmp(token, "btc"))
        res = btc_encryptwallet(pass);
    else
    if (0 == strcasecmp(token, "ltc"))
        res = ltc_encryptwallet(pass);
    return res;
}

static void walletlock (const char *token) {
    if (0 == strcmp(token, "btc"))
        btc_walletlock();
    else
    if (0 == strcmp(token, "ltc"))
        ltc_walletlock();
}

static void walletpassphrase (const char *token, const char *pass, int timeout) {
    if (0 == strcmp(token, "btc"))
        btc_walletpassphrase(pass, timeout);
    else
    if (0 == strcmp(token, "ltc"))
        ltc_walletpassphrase(pass, timeout);
}

int main (int argc, const char *argv[]) {
    if (argc < 2) {
        printf("command ?\n");
        return 1;
    }
    const char *cmd = argv[1];
    curl_global_init(CURL_GLOBAL_ALL);
    btc_load_conf(NULL);
    if (0 == strcmp(cmd, "newaccount")) {
        if (argc > 3) {
            const char *token = argv[2],
                       *arg2 = argv[3];
            create_addresses(token, arg2);
        }
    }
    else
    if (0 == strcmp(cmd, "listaccounts")) {
        if (argc > 2) {
            const char *token = argv[2];
            list_accounts(token);
        }
    } else
    if (0 == strcmp(cmd, "send")) {
        if (argc > 5) {
            const char *token = argv[2],
                       *from = argv[3],
                       *to = argv[4],
                       *amount = argv[5],
                       *pass = argv[6];
            send_tokens(token, from, to, amount, pass);
        }
    } else
    if (0 == strcmp(cmd, "encryptwallet")) {
        if (argc > 3) {
            const char *token = argv[2],
                       *pass = argv[3];
            char *msg = encryptwallet(token, pass);
            if (msg) {
                printf("%s\n", msg);
                free(msg);
            }
        }
    } else
    if (0 == strcmp(cmd, "walletlock")) {
        if (argc > 2) {
            const char *token = argv[2];
            walletlock(token);
        }
    } else
    if (0 == strcmp(cmd, "walletpassphrase")) {
        if (argc > 4) {
            const char *token = argv[2],
                       *pass = argv[3],
                       *s_timeout = argv[4];
            char *tail;
            int timeout = strtol(s_timeout, &tail, 0);
            if ('\0' == *tail && ERANGE != errno)
                walletpassphrase(token, pass, timeout);
        }
    } else
        printf("command ?\n");
    curl_global_cleanup();
    return 0;
}
