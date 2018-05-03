#include "coin.h"

#define COIN_DEC_STR "100000000"
#define COIN_DEC 100000000
mpz_t coin_dec;

static void init () __attribute__ ((constructor));
static void done () __attribute__ ((destructor));
static void init () {
    mpz_init_set_str(coin_dec, COIN_DEC_STR, 10);
}
static void done () {
    mpz_clear(coin_dec);
}

char *coin_double_to_int_str (double amount) {
    mpf_t f;
    mpz_t z;
    mpf_init(f);
    mpf_set_d(f, amount);
    mpf_mul_ui(f, f, COIN_DEC);
    mpz_init(z);
    mpz_set_f(z, f);
    char *res = mpz_get_str(NULL, 10, z);
    mpz_clear(z);
    mpf_clear(f);
    return res;
}

static void set_error_msg () {
    if (coin_errcode > 0) {
        switch (coin_errcode) {
            case COIN_NOJSON:
                strcpy(coin_errmsg, "No json"); break;
            case COIN_INVJSON:
                strcpy(coin_errmsg, "Invalid json"); break;
            case COIN_NOMETHOD:
                strcpy(coin_errmsg, "Unknown method"); break;
            default:
                strcpy(coin_errmsg, curl_easy_strerror(coin_errcode));
                break;
        }
    }
}

static size_t write_callback (char *ptr, size_t size, size_t nmemb, strbuf_t *buf) {
    size_t len = size * nmemb;
    strbufadd(buf, ptr, len);
    return len;
}

typedef size_t (*write_callback_h) (char*, size_t, size_t, void*);
static json_t *do_rpc (CURL *curl, chain_conf_t *conf, strbuf_t *buf, json_item_t **result) {
    json_t *json = NULL;
    json_item_t *ji, *j;
    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    coin_errcode = 0;
    coin_errmsg[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_URL, conf->host->ptr);
    curl_easy_setopt(curl, CURLOPT_USERNAME, conf->rpc_user ? conf->rpc_user->ptr : NULL);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, conf->rpc_pass ? conf->rpc_pass->ptr : NULL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buf->len);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf->ptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (write_callback_h)write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    buf->len = 0;
    if (CURLE_OK != (coin_errcode = curl_easy_perform(curl)))
        goto done;
    buf->ptr[buf->len] = '\0';
    if (!(json = json_parse_len(buf->ptr, buf->len))) {
        coin_errcode = COIN_NOJSON;
        goto done;
    }
    coin_errcode = COIN_INVJSON;
    if (JSON_OBJECT != json->type)
        goto done;
    if (!(ji = json_find(json->data.o, CONST_STR_LEN("result"), -1)))
        goto done;
    if (JSON_NULL != ji->type) {
        coin_errcode = 0;
        *result = ji;
        goto done;
    }
    if (!(ji = json_find(json->data.o, CONST_STR_LEN("error"), JSON_OBJECT)))
        goto done;
    if (!(j = json_find(ji->data.o, CONST_STR_LEN("message"), JSON_STRING)))
        goto done;
    strncpy(coin_errmsg, j->data.s.ptr, j->data.s.len < sizeof(coin_errmsg)-1 ? j->data.s.len : sizeof(coin_errmsg)-1);
    if (!(j = json_find(ji->data.o, CONST_STR_LEN("code"), JSON_INTEGER)))
        goto done;
    coin_errcode = j->data.i;
done:
    curl_slist_free_all(hdrs);
    if (0 != coin_errcode) {
        set_error_msg();
        if (json) {
            json_free(json);
            json = NULL;
        }
    }
    return json;
}

static void query_open (strbuf_t *buf, const char *method, size_t method_len) {
    buf->len = 0;
    json_begin_object(buf);
    json_add_str(buf, CONST_STR_LEN("jsonrpc"), CONST_STR_LEN("1.0"), JSON_NEXT);
    json_add_str(buf, CONST_STR_LEN("id"), CONST_STR_LEN("cli"), JSON_NEXT);
    json_add_str(buf, CONST_STR_LEN("method"), method, method_len, JSON_NEXT);
    json_open_array(buf, CONST_STR_LEN("params"));
}

static void query_close (strbuf_t *buf) {
    json_close_array(buf, JSON_END);
    json_close_object(buf, JSON_END);
}

#define PREPARE_EXEC \
    strbuf_t buf; \
    CURL *curl = curl_easy_init(); \
    json_t *json = NULL; \
    json_item_t *jr = NULL; \
    strbufalloc(&buf, 64, 64); \
    coin_errcode = 0; \
    coin_errmsg[0] = '\0';

#define DONE_EXEC \
    if (json) json_free(json); \
    free(buf.ptr); \
    curl_easy_cleanup(curl); 

char *coin_getnewaddress (chain_conf_t *conf, const char *account) {
    PREPARE_EXEC
    char *ret = NULL;
    query_open(&buf, CONST_STR_LEN("getnewaddress"));
    json_add_str(&buf, CONST_STR_NULL, account, strlen(account), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_STRING == jr->type)
            ret = strndup(jr->data.s.ptr, jr->data.s.len);
        else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return ret;
}

double coin_getbalance (chain_conf_t *conf, const char *account) {
    PREPARE_EXEC
    double ret = 0.0;
    query_open(&buf, CONST_STR_LEN("getbalance"));
    if (account)
        json_add_str(&buf, CONST_STR_NULL, account, strlen(account), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_DOUBLE == jr->type)
            ret = jr->data.d;
        else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return ret;
}

char *coin_getbalance_as_str (chain_conf_t *conf, const char *account) {
    double amount = coin_getbalance(conf, account);
    if (0 == coin_errcode) {
        char buf [256];
        snprintf(buf, sizeof(buf), "%.*f", 8, amount);
        return strdup(buf);
    }
    return NULL;
}

int coin_listtransactions (chain_conf_t *conf, const char *account, int from, int count, json_item_h fn, void *userdata, int flags) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("listtransactions"));
    if (account) {
        json_add_str(&buf, CONST_STR_NULL, account, strlen(account), count > 0 ? JSON_NEXT : JSON_END);
        if (count > 0) {
            json_add_int(&buf, CONST_STR_NULL, count, from >= 0 ? JSON_NEXT : JSON_END);
            if (from >= 0)
                json_add_int(&buf, CONST_STR_NULL, from, JSON_END);
        }
    }
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_ARRAY == jr->type) {
            json_enum_array(jr->data.a, fn, userdata, flags);
            rc = 0;
        } else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return rc;
}

int coin_listaccounts (chain_conf_t *conf, json_item_h fn, void *userdata, int flags) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("listaccounts"));
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_OBJECT == jr->type) {
            json_enum_object(jr->data.o, fn, userdata, flags);
            rc = 0;
        } else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return rc;
}

int coin_getaddressesbyaccount (chain_conf_t *conf, const char *account, json_item_h fn, void *userdata, int flags) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("getaddressesbyaccount"));
    json_add_str(&buf, CONST_STR_NULL, account, strlen(account), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_ARRAY == jr->type) {
            json_enum_array(jr->data.a, fn, userdata, flags);
            rc = 0;
        } else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return rc;
}

int coin_gettransaction (chain_conf_t *conf, const char *txid, json_item_h fn, void *userdata) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("gettransaction"));
    json_add_str(&buf, CONST_STR_NULL, txid, strlen(txid), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr))) {
        if (JSON_OBJECT == jr->type) 
            rc = fn(jr, userdata);
        else {
            coin_errcode = COIN_INVJSON;
            set_error_msg();
        }
    }
    DONE_EXEC
    return rc;
}

static str_t *coin_mpz_to_str (mpz_srcptr amount) {
    str_t *s_amount = stralloc(256, 64);
    mp_exp_t exp;
    mpf_t f;
    mpf_init(f);
    mpf_set_z(f, amount);
    mpf_div_ui(f, f, COIN_DEC);
    mpf_get_str(s_amount->ptr, &exp, 10, 0, f);
    s_amount->len = strlen(s_amount->ptr);
    if (exp > 0) {
        if (exp == s_amount->len)
            strnadd(&s_amount, CONST_STR_LEN(".0"));
        else
            strepl(&s_amount, s_amount->ptr + exp, 0, CONST_STR_LEN("."));
    } else
        strepl(&s_amount, s_amount->ptr, 0, CONST_STR_LEN("0."));
    return s_amount;
}

// sendfrom() will be removed in a later version of Bitcoin Core.
char *coin_sendfrom_z (chain_conf_t *conf, const char *from_account, const char *to_address, mpz_srcptr amount) {
    PREPARE_EXEC
    char *txid = NULL;
    str_t *s_amount = coin_mpz_to_str(amount);
    query_open(&buf, CONST_STR_LEN("sendfrom"));
    json_add_str(&buf, CONST_STR_NULL, from_account, strlen(from_account), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, to_address, strlen(to_address), JSON_NEXT);
    strbufadd(&buf, s_amount->ptr, s_amount->len);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        txid = strndup(jr->data.s.ptr, jr->data.s.len);
    DONE_EXEC
    free(s_amount);
    return txid;
}

char *coin_sendtoaddress_z (chain_conf_t *conf, const char *to_address, mpz_srcptr amount) {
    PREPARE_EXEC
    char *txid = NULL;
    str_t *s_amount = coin_mpz_to_str(amount);
    query_open(&buf, CONST_STR_LEN("sendtoaddress"));
    json_add_str(&buf, CONST_STR_NULL, to_address, strlen(to_address), JSON_NEXT);
    strbufadd(&buf, s_amount->ptr, s_amount->len);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        txid = strndup(jr->data.s.ptr, jr->data.s.len);
    DONE_EXEC
    free(s_amount);
    return txid;
}

char *coin_encryptwallet (chain_conf_t *conf, const char *pass) {
    PREPARE_EXEC
    char *res = NULL;
    query_open(&buf, CONST_STR_LEN("encryptwallet"));
    json_add_str(&buf, CONST_STR_NULL, pass, strlen(pass), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        res = strndup(jr->data.s.ptr, jr->data.s.len);
    DONE_EXEC
    return res;
}

void coin_walletlock (chain_conf_t *conf) {
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("walletlock"));
    query_close(&buf);
    json = do_rpc(curl, conf, &buf, &jr);
    DONE_EXEC
}

void coin_walletpassphrase (chain_conf_t *conf, const char *pass, int timeout) {
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("walletpassphrase"));
    json_add_str(&buf, CONST_STR_NULL, pass, strlen(pass), JSON_NEXT);
    json_add_int(&buf, CONST_STR_NULL, timeout, JSON_END);
    query_close(&buf);
    json = do_rpc(curl, conf, &buf, &jr);
    DONE_EXEC
}

void coin_stop (chain_conf_t *conf) {
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("stop"));
    query_close(&buf);
    json = do_rpc(curl, conf, &buf, &jr);
    DONE_EXEC
}

//*************************************************************************************
//  Multichain
//*************************************************************************************
char *mch_getnewaddress (chain_conf_t *conf) {
    char *addr = NULL;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("getnewaddress"));
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        addr = strndup(jr->data.s.ptr, jr->data.s.len);
    DONE_EXEC
    return addr;
}

static int on_validaddress (json_item_t *ji, mch_address_t *res) {
    if (JSON_ISNAME(ji, "account") && JSON_STRING == ji->type && ji->data.s.len > 0)
        res->account = json_str(ji);
    else
    if (JSON_ISNAME(ji, "address") && JSON_STRING == ji->type && ji->data.s.len > 0)
        res->address = json_str(ji);
    else
    if (JSON_ISNAME(ji, "iscompressed") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISCOMPRESSED;
    else
    if (JSON_ISNAME(ji, "ismine") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISMINE;
    else
    if (JSON_ISNAME(ji, "isscript") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISSCRIPT;
    else
    if (JSON_ISNAME(ji, "isvalid") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISVALID;
    else
    if (JSON_ISNAME(ji, "iswatchonly") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISWATCHONLY;
    else
    if (JSON_ISNAME(ji, "pubkey") && JSON_STRING == ji->type && ji->data.s.len > 0)
        res->pubkey = json_str(ji);
    else
    if (JSON_ISNAME(ji, "synchronized") && JSON_TRUE == ji->type)
        res->flags |= MCA_ISSYCHRONIZED;
    return ENUM_CONTINUE;
}

mch_address_t *mch_validateaddress (chain_conf_t *conf, const char *address) {
    mch_address_t *res = NULL;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("validateaddress"));
    json_add_str(&buf, CONST_STR_NULL, address, strlen(address), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_OBJECT == jr->type) {
        res = calloc(1, sizeof(mch_address_t));
        json_enum_object(jr->data.o, (json_item_h)on_validaddress, res, 0);
    }
    DONE_EXEC
    return res;
}

void mch_address_free (mch_address_t *addr) {
    if (addr->account) free(addr->account);
    if (addr->address) free(addr->address);
    if (addr->pubkey) free(addr->pubkey);
    free(addr);
}

int mch_getaddressbalances (chain_conf_t *conf, const char *address, int miniconf, json_item_h fn, void *userdata, int flags) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("getaddressbalances"));
    json_add_str(&buf, CONST_STR_NULL, address, strlen(address), miniconf > 0 ? JSON_NEXT : JSON_END);
    if (miniconf > 0)
        json_add_int(&buf, CONST_STR_NULL, miniconf, JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_ARRAY == jr->type) {
        json_enum_array(jr->data.a, fn, userdata, flags);
        rc = 0;
    }
    DONE_EXEC
    return rc;
}

// FIXME : check units for presision
char *mch_issue (chain_conf_t *conf, const char *asset, const char *address, double amount, int digits, int can_issuemore) {
    char *txid = NULL;
    int is_exists = 0, is_valid = 0;
    size_t addr_len = strlen(address);
    mch_address_t *addr = mch_validateaddress(conf, address);
    if (!addr)
        return NULL;
    is_valid = (addr->flags & MCA_ISVALID);
    mch_address_free(addr);
    if (is_valid) {
        PREPARE_EXEC
        if (is_exists) {
            query_open(&buf, CONST_STR_LEN("issuemore"));
            json_add_str(&buf, CONST_STR_NULL, address, addr_len, JSON_NEXT);
            json_add_str(&buf, CONST_STR_NULL, asset, strlen(asset), JSON_NEXT);
            json_add_double_p(&buf, CONST_STR_NULL, amount, 8, JSON_END);
        } else {
            double units = 1.0;
            if (digits > 0 && digits <= MCH_MAX_DIGITS) {
                for (int i = 0; i < digits; ++i)
                    units /= 10;
            }
            query_open(&buf, CONST_STR_LEN("issue"));
            json_add_str(&buf, CONST_STR_NULL, address, addr_len, JSON_NEXT);
            json_open_object(&buf, CONST_STR_NULL);
            json_add_str(&buf, CONST_STR_LEN("name"), asset, strlen(asset), JSON_NEXT);
            if (can_issuemore)
                json_add_true(&buf, CONST_STR_LEN("open"), JSON_END);
            else
                json_add_false(&buf, CONST_STR_LEN("open"), JSON_END);
            json_close_object(&buf, JSON_NEXT);
            json_add_double(&buf, CONST_STR_NULL, amount, JSON_NEXT);
            json_add_double(&buf, CONST_STR_NULL, units, JSON_END);
        }
        query_close(&buf);
        if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
            txid = strndup(jr->data.s.ptr, jr->data.s.len);
        DONE_EXEC
    }
    return txid;
}

char *mch_sendfrom_d (chain_conf_t *conf, const char *from_address, const char *to_address, const char *asset, double amount) {
    char *txid = NULL;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("sendassetfrom"));
    json_add_str(&buf, CONST_STR_NULL, from_address, strlen(from_address), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, to_address, strlen(to_address), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, asset, strlen(asset), JSON_NEXT);
    json_add_double(&buf, CONST_STR_NULL, amount, JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        txid = strndup(jr->data.s.ptr, jr->data.s.len);
    DONE_EXEC
    return txid;
}

int mch_listaddresstransactions (chain_conf_t *conf, const char *address, int from, int count, json_item_h fn, void *userdata, int flags) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("listaddresstransactions"));
    json_add_str(&buf, CONST_STR_NULL, address, strlen(address), JSON_NEXT);
    json_add_int(&buf, CONST_STR_NULL, count, JSON_NEXT);
    json_add_int(&buf, CONST_STR_NULL, from, JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_ARRAY == jr->type) {
        fn(jr, userdata);
        rc = 0;
    }
    DONE_EXEC
    return rc;
}

int mch_getwallettransaction (chain_conf_t *conf, const char *txid, json_item_h fn, void *userdata) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("getwallettransaction"));
    json_add_str(&buf, CONST_STR_NULL, txid, strlen(txid), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_OBJECT == jr->type) {
        fn(jr, userdata);
        rc = 0;
    }
    DONE_EXEC
    return rc;
}

int mch_listpermissions (chain_conf_t *conf, const char *address, json_item_h fn, void *userdata) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("listpermissions"));
    if (address) {
        json_add_str(&buf, CONST_STR_NULL, CONST_STR_LEN("*"), JSON_NEXT);
        json_add_str(&buf, CONST_STR_NULL, address, strlen(address), JSON_END);
    }
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_ARRAY == jr->type) {
        fn(jr, userdata);
        rc = 0;
    }
    DONE_EXEC
    return rc;
}

int mch_grant (chain_conf_t *conf, const char *address, const char *permissions) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("grant"));
    json_add_str(&buf, CONST_STR_NULL, address, strlen(address), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, permissions, strlen(permissions), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        rc = 0;
    DONE_EXEC
    return rc;
}

int mch_revoke (chain_conf_t *conf, const char *address, const char *permissions) {
    int rc = -1;
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("revoke"));
    json_add_str(&buf, CONST_STR_NULL, address, strlen(address), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, permissions, strlen(permissions), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, conf, &buf, &jr)) && JSON_STRING == jr->type)
        rc = 0;
    DONE_EXEC
    return rc;
}
