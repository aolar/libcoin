#include "ether.h"

#define MAX_WEI_HEX "effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
#define WEI_IN_ETHER "1000000000000000000"
mpz_t max_wei;
mpz_t wei_in_ether,
      eth_gwei,
      eth_fast_price,
      eth_average_price,
      eth_slow_price;

static void init () __attribute__ ((constructor));
static void done () __attribute__ ((destructor));
static void init () {
    mpz_init_set_str(max_wei, MAX_WEI_HEX, 16);
    mpz_init_set_str(wei_in_ether, WEI_IN_ETHER, 10);
    mpz_init(eth_gwei);
    mpz_init(eth_fast_price);
    mpz_init(eth_average_price);
    mpz_init(eth_slow_price);
    mpz_set_ui(eth_gwei, 1000000000);
    mpz_set_ui(eth_fast_price, 40000000000);
    mpz_set_ui(eth_average_price, 4000000000);
    mpz_set_ui(eth_slow_price, 600000000);
}
static void done () {
    mpz_clear(eth_fast_price);
    mpz_clear(eth_average_price);
    mpz_clear(eth_slow_price);
    mpz_clear(eth_gwei);
    mpz_clear(wei_in_ether);
    mpz_clear(max_wei);
}

/*str_t *ether_str_to_int_str (const char *ether_amount) {
    str_t *res = NULL;
    char *p = strchr(ether_amount, '.');
    if (p) {
        size_t l = strlen(p+1), n = ETH_DSCALE - l;
        res = mkstr(ether_amount, (uintptr_t)p - (uintptr_t)ether_amount, ETH_DSCALE);
        strnadd(&res, p + 1, l);
        if (n > 0)
            strpad(&res, res->len + n, '0', STR_LEFT);
    } else {
        res = mkstr(ether_amount, strlen(ether_amount), ETH_DSCALE);
        strpad(&res, res->len + ETH_DSCALE, '0', STR_LEFT);
    }
    return res;
}*/

char *ether_hex_to_int_str (const char *amount) {
    mpz_t z;
    if (strlen(amount) < 3)
        return NULL;
    mpz_init_set_str(z, amount+2, 16);
    char *res = mpz_get_str(NULL, 10, z);
    mpz_clear(z);
    return res;
}

static int method_compare (const char *proto, eth_method_t *m) {
    return strcmp(proto, m->proto);
}

eth_contract_t *eth_declare_contract (const char *address) {
    eth_contract_t *contract = calloc(1, sizeof(eth_contract_t));
    contract->address = strdup(address);
    contract->methods = array_alloc(sizeof(eth_method_t), 8);
    contract->methods->on_compare = (compare_h)method_compare;
    return contract;
}

eth_method_t *eth_declare_method (eth_contract_t *contract, const char *proto, size_t proto_len) {
    str_t *sha = eth_sha3(proto, proto_len);
    eth_method_t *m = NULL;
    if (sha) {
        m = array_add(contract->methods, (void*)proto);
        memset(m, 0, sizeof(eth_method_t));
        m->proto = strdup(proto);
        m->sign = strndup(sha->ptr, 10);
        free(sha);
    }
    return m;
}

static str_t *get_proto (const char *name, size_t name_len, list_t *args) {
    list_item_t *li = args->head;
    str_t *proto = mkstr(name, name_len, 128);
    strnadd(&proto, CONST_STR_LEN("("));
    if (li) {
        int is_first = 1;
        do {
            json_item_t *j = (json_item_t*)li->ptr, *jt;
            if (JSON_OBJECT != j->type || !(jt = json_find(j->data.o, CONST_STR_LEN("type"), JSON_STRING))) {
                free(proto);
                coin_errcode = COIN_INVJSON;
                strcpy(coin_errmsg, "invalid input json");
                return NULL;
            }
            if (!is_first)
                strnadd(&proto, CONST_STR_LEN(","));
            strnadd(&proto, jt->data.s.ptr, jt->data.s.len);
            is_first = 0;
            li = li->next;
        } while (li != args->head);
    }
    strnadd(&proto, CONST_STR_LEN(")"));
    return proto;
}

static int get_outputs (eth_method_t *m, list_t *outputs) {
    if (0 == outputs->len)
        return 0;
    m->outputs = calloc(outputs->len, sizeof(char*));
    list_item_t *li = outputs->head;
    do {
        json_item_t *jo = (json_item_t*)li->ptr, *j_type;
        if (JSON_OBJECT != jo->type || !(j_type = json_find(jo->data.o, CONST_STR_LEN("type"), JSON_STRING))) {
            coin_errcode = COIN_INVJSON;
            strcpy(coin_errmsg, "invalid json");
            return -1;
        }
        m->outputs[m->output_len++] = strndup(j_type->data.s.ptr, j_type->data.s.len);
        li = li->next;
    } while (li != outputs->head);
    return 0;
}

static int declare_function (eth_contract_t *contract, json_item_t *jf) {
    str_t *proto;
    eth_method_t *m;
    json_item_t *j_type, *j_name, *j_state, *j_inputs, *j_payable, *j_outputs;
    if (!(j_type = json_find(jf->data.o, CONST_STR_LEN("type"), JSON_STRING)) ||
        !(j_name = json_find(jf->data.o, CONST_STR_LEN("name"), JSON_STRING)) ||
        !(j_state = json_find(jf->data.o, CONST_STR_LEN("stateMutability"), JSON_STRING)) ||
        !(j_payable = json_find(jf->data.o, CONST_STR_LEN("payable"), -1)) ||
        !(j_inputs = json_find(jf->data.o, CONST_STR_LEN("inputs"), JSON_ARRAY)) ||
        !(j_outputs = json_find(jf->data.o, CONST_STR_LEN("outputs"), JSON_ARRAY)) ||
        (JSON_TRUE != j_payable->type && JSON_FALSE != j_payable->type))
            return 0;
    if (0 != cmpstr(j_type->data.s.ptr, j_type->data.s.len, CONST_STR_LEN("function")))
        return 0;
    if (!(proto = get_proto(j_name->data.s.ptr, j_name->data.s.len, j_inputs->data.a)))
        return -1;
    if (!(m = eth_declare_method(contract, proto->ptr, proto->len))) {
        free(proto);
        return -1;
    }
    free(proto);
    if (0 == cmpstr(j_state->data.s.ptr, j_state->data.s.len, CONST_STR_LEN("view")))
        m->state = ST_VIEW;
    else
    if (0 == cmpstr(j_state->data.s.ptr, j_state->data.s.len, CONST_STR_LEN("nonpayable")))
        m->state = ST_NONPAYABLE;
    if (JSON_TRUE == j_payable->type)
        m->is_payable = 1;
    return get_outputs(m, j_outputs->data.a);
}

eth_contract_t *eth_load_contract (const char *address, const char *intf, size_t intf_len) {
    eth_contract_t *contract = NULL;
    json_t *json = json_parse_len(intf, intf_len);
    list_item_t *li;
    coin_errcode = 0;
    *coin_errmsg = '\0';
    if (!json) {
        coin_errcode = COIN_NOJSON;
        strcpy(coin_errmsg, "input isn't json");
        goto err;
    }
    if (JSON_ARRAY != json->type) {
        coin_errcode = COIN_INVJSON;
        strcpy(coin_errmsg, "invalid json");
        goto err;
    }
    contract = eth_declare_contract(address);
    if ((li = json->data.a->head)) {
        do {
            json_item_t *jf = (json_item_t*)li->ptr;
            if (-1 == declare_function(contract, jf))
                goto err;
            li = li->next;
        } while (li != json->data.a->head);
    }
    json_free(json);
    return contract;
err:
    if (contract)
        eth_free_contract(contract);
    if (json)
        json_free(json);
    return NULL;
}

void eth_free_contract (eth_contract_t *contract) {
    free(contract->address);
    for (size_t i = 0; i < contract->methods->len; ++i) {
        eth_method_t *m = contract->methods->data + (i * contract->methods->data_size);
        free(m->proto);
        free(m->sign);
        if (m->outputs) {
            for (size_t j = 0; j < m->output_len; ++j)
                free(m->outputs[j]);
            free(m->outputs);
        }
    }
    array_free(contract->methods);
    free(contract);
}

static size_t write_callback (char *ptr, size_t size, size_t nmemb, strbuf_t *buf) {
    size_t len = size * nmemb;
    strbufadd(buf, ptr, len);
    return len;
}

typedef size_t (*write_callback_h) (char*, size_t, size_t, void*);
static json_t *do_rpc (CURL *curl, strbuf_t *buf, json_item_t **j_result) {
    json_t *json = NULL;
    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    coin_errcode = COIN_SUCCESS;
    *coin_errmsg = '\0';
    curl_easy_setopt(curl, CURLOPT_URL, eth_conf.host->ptr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buf->len);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf->ptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (write_callback_h)write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    buf->len = 0;
    if (CURLE_OK == (coin_errcode = curl_easy_perform(curl))) {
        buf->ptr[buf->len] = '\0';
        if ((json = json_parse_len(buf->ptr, buf->len))) {
            if (JSON_OBJECT == json->type) {
                json_item_t *j_err = json_find(json->data.o, CONST_STR_LEN("error"), JSON_OBJECT), *j_code, *j_msg;
                if (j_err &&
                    (j_code = json_find(j_err->data.o, CONST_STR_LEN("code"), JSON_INTEGER)) &&
                    (j_msg = json_find(j_err->data.o, CONST_STR_LEN("message"), JSON_STRING))) {
                    coin_errcode = j_code->data.i;
                    strncpy(coin_errmsg, j_msg->data.s.ptr, j_msg->data.s.len < sizeof(coin_errmsg)-1 ? j_msg->data.s.len : sizeof(coin_errmsg)-1);
                    json_free(json);
                    json = NULL;
                } else {
                    if (!(*j_result = json_find(json->data.o, CONST_STR_LEN("result"), -1))) {
                        json_free(json);
                        json = NULL;
                        coin_errcode = COIN_INVJSON;
                        strcpy(coin_errmsg, "ivalid json");
                    }
                }
            } else {
                json_free(json);
                json = NULL;
                coin_errcode = COIN_INVJSON;
                strcpy(coin_errmsg, "ivalid json");
            }
        } else {
            coin_errcode = COIN_NOJSON;
            strcpy(coin_errmsg, "result isn't json");
        }
    } else
        strcpy(coin_errmsg, curl_easy_strerror(coin_errcode));
    curl_slist_free_all(hdrs);
    return json;
}

static void query_open (strbuf_t *buf, const char *method, size_t method_len) {
    buf->len = 0;
    json_begin_object(buf);
    json_add_int(buf, CONST_STR_LEN("id"), 1, JSON_NEXT);
    json_add_str(buf, CONST_STR_LEN("jsonrpc"), CONST_STR_LEN("2.0"), JSON_NEXT);
    json_add_str(buf, CONST_STR_LEN("method"), method, method_len, JSON_NEXT);
    json_open_array(buf, CONST_STR_LEN("params"));
}

static void query_close (strbuf_t *buf) {
    json_close_array(buf, JSON_END);
    json_close_object(buf, JSON_END);
}

int eth_unlock_account (const char *account, const char *passwd, int timeout) {
    int rc = -1;
    CURL *curl = curl_easy_init();
    strbuf_t buf;
    json_item_t *j_result;
    strbufalloc(&buf, 64, 64);
    query_open(&buf, CONST_STR_LEN("personal_unlockAccount"));
    json_add_str(&buf, CONST_STR_NULL, account, strlen(account), JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, passwd, strlen(passwd), JSON_NEXT);
    json_add_int(&buf, CONST_STR_NULL, timeout <= 0 ? ETH_UNLOCK_TIMEOUT : timeout, JSON_END);
    query_close(&buf);
    json_t *json = do_rpc(curl, &buf, &j_result);
    if (json) {
        if (j_result)
            rc = JSON_TRUE == j_result->type ? 1 : 0;
        json_free(json);
    }
    free(buf.ptr);
    curl_easy_cleanup(curl);
    return rc;
}

char *eth_get_coinbase (char *address, size_t address_len) {
    char *res = NULL;
    json_t *json;
    json_item_t *j_result;
    strbuf_t buf;
    CURL *curl = curl_easy_init();
    strbufalloc(&buf, 64, 64);
    query_open(&buf, CONST_STR_LEN("eth_coinbase"));
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &j_result))) {
        if (j_result && JSON_STRING == j_result->type) {
            size_t len = j_result->data.s.len < address_len ? j_result->data.s.len : address_len;
            strncpy(address, j_result->data.s.ptr, len);
            address[len] = '\0';
            res = address;
        }
        json_free(json);
    }
    free(buf.ptr);
    curl_easy_cleanup(curl);
    return res;
}

char *eth_new_account (const char *pass, char *address, size_t address_len) {
    char *res = NULL;
    json_t *json;
    json_item_t *j_result;
    strbuf_t buf;
    CURL *curl = curl_easy_init();
    strbufalloc(&buf, 64, 64);
    query_open(&buf, CONST_STR_LEN("personal_newAccount"));
    json_add_str(&buf, CONST_STR_NULL, pass, strlen(pass), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &j_result))) {
        if (j_result && JSON_STRING == j_result->type) {
            size_t len = j_result->data.s.len < address_len ? j_result->data.s.len : address_len;
            strncpy(address, j_result->data.s.ptr, len);
            address[len] = '\0';
            res = address;
        }
        json_free(json);
    }
    free(buf.ptr);
    curl_easy_cleanup(curl);
    return res;
}

str_t *eth_sha3 (const char *param, size_t param_len) {
    json_t *json;
    json_item_t *j_result;
    strbuf_t buf;
    str_t *str = strhex("0x", param, param_len, 8), *result = NULL;
    CURL *curl = curl_easy_init();
    strbufalloc(&buf, 256, 64);
    query_open(&buf, CONST_STR_LEN("web3_sha3"));
    json_add_str(&buf, CONST_STR_NULL, str->ptr, str->len, JSON_END);
    query_close(&buf);
    free(str);
    if ((json = do_rpc(curl, &buf, &j_result))) {
        if (j_result && JSON_STRING == j_result->type)
            result = mkstr(j_result->data.s.ptr, j_result->data.s.len, 8);
        json_free(json);
    }
    free(buf.ptr);
    curl_easy_cleanup(curl);
    return result;
}

void eth_ctx_clear (eth_ctx_t *ctx) {
    if (ctx->buf.ptr) free(ctx->buf.ptr);
    if (ctx->json) json_free(ctx->json);
    if (ctx->result) free(ctx->result);
}

int eth_prepare_exec (eth_contract_t *contract, const char *from, const char *proto, eth_ctx_t *ctx) {
    array_t *methods = contract->methods;
    ssize_t idx = array_find(methods, (void*)proto);
    if (-1 == idx) {
        coin_errcode = COIN_NOMETHOD;
        snprintf(coin_errmsg, sizeof coin_errmsg, "unknown contract method %s", proto);
        return -1;
    }
    eth_method_t *m = (eth_method_t*)(methods->data + idx * methods->data_size);
    strbuf_t *buf = &ctx->buf;
    strbufalloc(buf, 256, 256);
    if (ST_VIEW == m->state)
        query_open(buf, CONST_STR_LEN("eth_call"));
    else
        query_open(buf, CONST_STR_LEN("eth_sendTransaction"));
    json_open_object(buf, CONST_STR_NULL);
    json_add_str(buf, CONST_STR_LEN("from"), from, strlen(from), JSON_NEXT);
    json_add_str(buf, CONST_STR_LEN("to"), contract->address, strlen(contract->address), JSON_NEXT);
    json_add_key(buf, CONST_STR_LEN("data"));
    strbufadd(buf, CONST_STR_LEN("\""));
    strbufadd(buf, m->sign, strlen(m->sign));
    ctx->contract = contract;
    ctx->method = m;
    return 0;
}

void eth_param_address (eth_ctx_t *ctx, const char *address) {
    str_t *str = mkstr(address+2, strlen(address)-2, 64);
    strpad(&str, 64, '0', STR_RIGHT);
    strbufadd(&ctx->buf, str->ptr, str->len);
    free(str);
}

void eth_param_int (eth_ctx_t *ctx, mpz_t x) {
    char *s = mpz_get_str(NULL, 16, x);
    str_t *str = mkstr(s, strlen(s), 64);
    free(s);
    strpad(&str, 64, '0', STR_RIGHT);
    strbufadd(&ctx->buf, str->ptr, str->len);
    free(str);
}

int eth_exec (eth_ctx_t *ctx, const char *value, const char *gas, const char *gas_price) {
    CURL *curl = curl_easy_init();
    int rc = -1;
    if (ST_VIEW == ctx->method->state) {
        strbufadd(&ctx->buf, CONST_STR_LEN("\""));
        json_close_object(&ctx->buf, JSON_NEXT);
        json_add_str(&ctx->buf, CONST_STR_NULL, CONST_STR_LEN("latest"), JSON_END);
        query_close(&ctx->buf);
        ctx->call_type = ETH_CALL;
    } else {
        strbufadd(&ctx->buf, CONST_STR_LEN("\""));
        if (value) {
            strbufadd(&ctx->buf, CONST_STR_LEN(","));
            json_add_str(&ctx->buf, CONST_STR_LEN("value"), value, strlen(value), JSON_END);
        }
        if (gas) {
            strbufadd(&ctx->buf, CONST_STR_LEN(","));
            json_add_str(&ctx->buf, CONST_STR_LEN("gas"), gas, strlen(gas), JSON_END);
        }
        if (gas_price) {
            strbufadd(&ctx->buf, CONST_STR_LEN(","));
            json_add_str(&ctx->buf, CONST_STR_LEN("gasPrice"), gas_price, strlen(gas_price), JSON_END);
        }
        json_close_object(&ctx->buf, JSON_END);
        query_close(&ctx->buf);
        ctx->call_type = ETH_SENDTRAN;
    }
    json_item_t *j_result = NULL;
    if ((ctx->json = do_rpc(curl, &ctx->buf, &j_result)) && (j_result)) {
        ctx->result = mkstr(j_result->data.s.ptr, j_result->data.s.len, 8);
        rc = 0;
    }
    return rc;
}

#define PREPARE_EXEC \
    CURL *curl = curl_easy_init(); \
    strbuf_t buf; \
    json_t *json; \
    json_item_t *jr = NULL; \
    strbufalloc(&buf, 64, 64); \
    coin_errcode = 0; \
    coin_errmsg[0] = '\0';

#define DONE_EXEC \
    free(buf.ptr); \
    curl_easy_cleanup(curl);

char *eth_send (const char *from, const char *to, const char *amount, const char *gas, const char *gas_price) {
    PREPARE_EXEC
    char *txid = NULL;
    query_open(&buf, CONST_STR_LEN("eth_sendTransaction"));
    json_open_object(&buf, CONST_STR_NULL);
    json_add_str(&buf, CONST_STR_LEN("from"), from, strlen(from), JSON_NEXT);
    json_add_str(&buf, CONST_STR_LEN("to"), to, strlen(to), JSON_NEXT);
    json_add_str(&buf, CONST_STR_LEN("value"), amount, strlen(amount), gas ? JSON_NEXT : JSON_END);
    if (gas)
        json_add_str(&buf, CONST_STR_LEN("gas"), gas, strlen(gas), gas_price ? JSON_NEXT : JSON_END);
    if (gas_price)
        json_add_str(&buf, CONST_STR_LEN("gasPrice"), gas_price, strlen(gas_price), JSON_END);
    json_close_object(&buf, JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_STRING == jr->type)
            txid = strndup(jr->data.s.ptr, jr->data.s.len);
        json_free(json);
    }
    DONE_EXEC
    return txid;
}

char *eth_newaccount (const char *passwd) {
    PREPARE_EXEC
    char *addr = NULL;
    query_open(&buf, CONST_STR_LEN("personal_newAccount"));
    json_add_str(&buf, CONST_STR_NULL, passwd, strlen(passwd), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_STRING == jr->type)
            addr = strndup(jr->data.s.ptr, jr->data.s.len);
        json_free(json);
    }
    DONE_EXEC
    return addr;
}

char *eth_coinbase () {
    PREPARE_EXEC
    char *addr = NULL;
    query_open(&buf, CONST_STR_LEN("eth_coinbase"));
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_STRING == jr->type)
            addr = strndup(jr->data.s.ptr, jr->data.s.len);
        json_free(json);
    }
    DONE_EXEC
    return addr;
}

char *eth_send_wei (const char *from, const char *to, mpz_srcptr amount, mpz_srcptr gas, mpz_srcptr gas_price) {
    char s_amount [256] = "0x", s_gas [256] = "0x0", s_price [256] = "0x0";
    mpz_get_str(s_amount+2, 16, amount);
    if (gas) mpz_get_str(s_gas+2, 16, gas);
    if (gas_price) mpz_get_str(s_price+2, 16, gas_price);
    return eth_send(from, to, s_amount, s_gas, s_price);
}

char *eth_send_ether (const char *from, const char *to, mpq_srcptr amount, mpz_srcptr gas, mpz_srcptr gas_price) {
    mpq_t q;
    mpq_init(q);
    mpq_set_z(q, wei_in_ether);
    mpq_mul(q, q, amount);
    char *txid = eth_send_wei(from, to, &q->_mp_num, gas , gas_price);
    mpq_clear(q);
    return txid;
}

char *eth_getbalance (const char *address, size_t address_len) {
    PREPARE_EXEC
    char *res = NULL;
    query_open(&buf, CONST_STR_LEN("eth_getBalance"));
    json_add_str(&buf, CONST_STR_NULL, address, address_len, JSON_NEXT);
    json_add_str(&buf, CONST_STR_NULL, CONST_STR_LEN("latest"), JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_STRING == jr->type)
            res = strndup(jr->data.s.ptr, jr->data.s.len);
        json_free(json);
    }
    DONE_EXEC
    return res;
}

void eth_rawstr_to_wei (const char *rawstr, mpz_t amount) {
    mpz_init(amount);
    mpz_set_str(amount, rawstr+2, 16);
}

void eth_rawstr_to_ether (const char *rawstr, mpf_t amount) {
    mpz_t z;
    mpq_t qx, qy;
    eth_rawstr_to_wei(rawstr, z);
    mpq_init(qx);
    mpq_init(qy);
    mpq_set_z(qx, z);
    mpz_set_str(z, WEI_IN_ETHER, 10);
    mpq_set_z(qy, z);
    mpq_div(qx, qx, qy);
    mpf_init(amount);
    mpf_set_q(amount, qx);
    mpz_clear(z);
    mpq_clear(qx);
    mpq_clear(qy);
}

int eth_accounts (json_item_h fn, void *userdata, int flags) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("eth_accounts"));
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_ARRAY == jr->type) {
            json_enum_array(jr->data.a, fn, userdata, flags);
            rc = 0;
        }
        json_free(json);
    }
    DONE_EXEC
    return rc;
}

int eth_gettransaction_by_hash (const char *hash, size_t hash_len, json_item_h fn, void *userdata) {
    PREPARE_EXEC
    int rc = -1;
    query_open(&buf, CONST_STR_LEN("eth_getTransactionByHash"));
    json_add_str(&buf, CONST_STR_NULL, hash, hash_len, JSON_END);
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr))) {
        if (JSON_OBJECT == jr->type)
            rc = fn(jr, userdata);
    }
    DONE_EXEC
    return rc;
}

void eth_mine () {
    PREPARE_EXEC
    query_open(&buf, CONST_STR_LEN("miner_start"));
    query_close(&buf);
    if ((json = do_rpc(curl, &buf, &jr)))
        json_free(json);
    DONE_EXEC
}
