#include "conf.h"

__thread int coin_errcode = 0;
__thread char coin_errmsg [256] = { '\0' };

chain_conf_t btc_conf, *btc_conf_ptr = &btc_conf;
chain_conf_t ltc_conf, *ltc_conf_ptr = &ltc_conf;
chain_conf_t eth_conf;
mch_conf_t *mch_conf = NULL;

static void init () __attribute__ ((constructor));
static void done () __attribute__ ((destructor));

void coin_update_host (chain_conf_t *conf) {
    size_t start_len = conf->rpc_host->len + conf->rpc_prot->len + 16;
    if (conf->host) free(conf->host);
    conf->host = stralloc(start_len, 8);
    conf->host->len = snprintf(conf->host->ptr, start_len, "%s://%s:%d", conf->rpc_prot->ptr, conf->rpc_host->ptr, conf->rpc_port);
}

static void free_conf (chain_conf_t *conf, uintptr_t is_persistent) {
    if (conf->name) free(conf->name);
    if (conf->chain_protocol) free(conf->chain_protocol);
    if (conf->rpc_host) free(conf->rpc_host);
    if (conf->rpc_prot) free(conf->rpc_prot);
    if (conf->host) free(conf->host);
    if (conf->rpc_user) free(conf->rpc_user);
    if (conf->rpc_pass) free(conf->rpc_pass);
    if (!is_persistent)
        free(conf);
}

static void init () {
    memset(&btc_conf, 0, sizeof(chain_conf_t));
    memset(&ltc_conf, 0, sizeof(chain_conf_t));
    memset(&eth_conf, 0, sizeof(chain_conf_t));

    btc_conf.rpc_port = DEFAULT_BTC_PORT;
    btc_conf.rpc_host = mkstr(CONST_STR_LEN(DEFAULT_HOST), 8);
    btc_conf.rpc_prot = mkstr(CONST_STR_LEN(DEFAULT_PROT), 8);
    coin_update_host(&btc_conf);

    ltc_conf.rpc_port = DEFAULT_LTC_PORT;
    ltc_conf.rpc_host = mkstr(CONST_STR_LEN(DEFAULT_HOST), 8);
    ltc_conf.rpc_prot = mkstr(CONST_STR_LEN(DEFAULT_PROT), 8);
    coin_update_host(&ltc_conf);

    eth_conf.rpc_port = DEFAULT_ETH_PORT;
    eth_conf.rpc_host = mkstr(CONST_STR_LEN(DEFAULT_HOST), 8);
    eth_conf.rpc_prot = mkstr(CONST_STR_LEN(DEFAULT_PROT), 8);
    coin_update_host(&eth_conf);
    
    mch_conf = lst_alloc((free_h)free_conf);
}

static void done () {
    free_conf(&btc_conf, 1);
    free_conf(&ltc_conf, 1);
    free_conf(&eth_conf, 1);
    lst_free(mch_conf);
}

static int coin_load_conf (const char *fname, chain_conf_t *conf) {
    str_t *user = NULL, *pass = NULL;
    int port = -1, rc = -1;
    load_conf_exactly(fname, CONF_HANDLER
        ASSIGN_CONF_STR(user, "rpcuser")
        ASSIGN_CONF_STR(pass, "rpcpassword")
        ASSIGN_CONF_INT(port, "rpcport")
    CONF_HANDLER_END);
    if (user && pass)
        rc = 0;
    if (0 == rc) {
        if (conf->rpc_user)
            free(conf->rpc_user);
        conf->rpc_user = user;
        if (conf->rpc_pass)
            free(conf->rpc_pass);
        conf->rpc_pass = pass;
        if (port > 0) {
            conf->rpc_port = port;
            coin_update_host(conf);
        }
    } else {
        if (user) free(user);
        if (pass) free(pass);
    }
    return rc;
}

int btc_load_conf (const char *home) {
    str_t *fname = path_combine(home ? home : getenv("HOME"), ".bitcoin", "bitcoin.conf", NULL);
    int rc = coin_load_conf(fname->ptr, &btc_conf);
    free(fname);
    return rc;
}

int ltc_load_conf (const char *home) {
    str_t *fname = path_combine(home ? home : getenv("HOME"), ".litecoin", "litecoin.conf", NULL);
    int rc = coin_load_conf(fname->ptr, &ltc_conf);
    free(fname);
    return rc;
}

void eth_set_conf (const char *rpc_prot, size_t rpc_prot_len, const char *rpc_host, size_t rpc_host_len, int rpc_port) {
    if (rpc_prot)
        strput(&eth_conf.rpc_prot, rpc_prot, rpc_prot_len, STR_REDUCE);
    if (rpc_host)
        strput(&eth_conf.rpc_host, rpc_host, rpc_host_len, STR_REDUCE);
    if (rpc_port > 0)
        eth_conf.rpc_port = rpc_port;
    coin_update_host(&eth_conf);
}

static void try_load_mconf(const char *dir, const char *chain_name) {
    struct stat st;
    str_t *conf_name = path_combine(dir, chain_name, "multichain.conf", NULL),
          *param_name = path_combine(dir, chain_name, "params.dat", NULL);
    if (-1 != stat(conf_name->ptr, &st) && -1 != stat(param_name->ptr, &st)) {
        int port = -1, target_block_time = 0, network_port = -1;
        chain_conf_t *conf = calloc(1, sizeof(chain_conf_t));
        conf->rpc_port = -1;
        load_conf_exactly(conf_name->ptr, CONF_HANDLER
            ASSIGN_CONF_STR(conf->rpc_user, "rpcuser")
            ASSIGN_CONF_STR(conf->rpc_pass, "rpcpassword")
        CONF_HANDLER_END);
        load_conf_exactly(param_name->ptr, CONF_HANDLER
            ASSIGN_CONF_INT(port, "default-rpc-port")
            ASSIGN_CONF_STR(conf->name, "chain-name")
            ASSIGN_CONF_STR(conf->chain_protocol, "chain-protocol")
            ASSIGN_CONF_INT(target_block_time, "target-block-time")
            ASSIGN_CONF_INT(network_port, "default-network-port");
        CONF_HANDLER_END);
        if (port > 0 && network_port > 0 && conf->rpc_user && conf->rpc_pass && conf->name && conf->chain_protocol) {
            conf->rpc_port = port;
            conf->network_port = network_port;
            conf->rpc_host = mkstr(CONST_STR_LEN(DEFAULT_HOST), 8);
            conf->rpc_prot = mkstr(CONST_STR_LEN(DEFAULT_PROT), 8);
            conf->target_block_time = target_block_time;
            lst_add(mch_conf, conf);
            coin_update_host(conf);
        } else
            free(conf);
    }
    free(param_name);
    free(conf_name);
}

void mch_load_conf (const char *home) {
    str_t *dir = path_combine(home ? home : getenv("HOME"), ".multichain", NULL);
    struct stat st;
    if (-1 != stat(dir->ptr, &st) && S_ISDIR(st.st_mode)) {
        DIR *dp = opendir(dir->ptr);
        if (dp) {
            struct dirent *ep;
            if (mch_conf)
                lst_clear(mch_conf);
            while ((ep = readdir(dp))) {
                if (DT_DIR == ep->d_type && '.' != *ep->d_name && 0 != strcmp(ep->d_name, "start"))
                    try_load_mconf(dir->ptr, ep->d_name);
            }
            closedir(dp);
        }
    }
    free(dir);
}

typedef struct {
    const char *name;
    size_t name_len;
    chain_conf_t *conf;
} mch_get_conf_t;

static int on_get_conf (list_item_t *li, mch_get_conf_t *gc) {
    chain_conf_t *cf = (chain_conf_t*)li->ptr;
    if (0 == cmpstr(cf->name->ptr, cf->name->len, gc->name, gc->name_len)) {
        gc->conf = cf;
        return ENUM_BREAK;
    }
    return ENUM_CONTINUE;
}

chain_conf_t *mch_get_conf (const char *name, size_t name_len) {
    mch_get_conf_t gc = { .name = name, .name_len = name_len, .conf = NULL };
    lst_enum(mch_conf, (list_item_h)on_get_conf, (void*)&gc, ENUM_STOP_IF_BREAK);
    if (!gc.conf) {
        coin_errcode = COIN_CONF_NOT_FOUND;
        if (name_len > 250)
            strcpy(coin_errmsg, "blockchain name not found");
        else {
            str_t *blockchain_name = mkstr(name, name_len, 8);
            snprintf(coin_errmsg, sizeof(coin_errmsg), "blockchain name '%s' not found", blockchain_name->ptr);
            free(blockchain_name);
        }
    }
    return gc.conf;
}

static int is_zero_str (const char *a, const char *b) {
    while (a < b) {
        if ('0' != *a++) return 0;
    }
    return 1;
}

str_t *str_to_int_str (const char *amount, int scale) {
    str_t *res = NULL;
    char *p = strchr(amount, '.');
    if (p) {
        size_t l = strlen(p+1), n = scale - l;
        if (is_zero_str(amount, p))
            res = stralloc(l, scale);
        else
            res = mkstr(amount, (uintptr_t)p - (uintptr_t)amount, scale);
        strnadd(&res, p + 1, l);
        if (n > 0)
            strpad(&res, res->len + n, '0', STR_LEFT);
    } else {
        res = mkstr(amount, strlen(amount), scale);
        strpad(&res, res->len + scale, '0', STR_LEFT);
    }
    return res;
}
