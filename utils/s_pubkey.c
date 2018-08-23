#include <libex/str.h>
#include <libex/file.h>
#include <libsslw/crypt.h>
#include "coin.h"

int main (int argc, const char *argv[]) {
    int rc = 1;
    if (argc != 3) return 1;
    const char *chain_name = argv[1],
               *addr = argv[2];
    chain_conf_t *mch;
    mch_load_conf(NULL);
    if (!(mch = mch_get_conf(chain_name, strlen(chain_name)))) return 1;
    if (0 != cr_init()) return 1;
    str_t *dir = get_spec_path(DIR_HOME);
    dir = path_add_path(dir, chain_name, "stream-privkeys", NULL);
    if (0 == mkdir(dir->ptr, 0775) || EEXIST != errno) goto done;
    cr_key_pair_t *keys = cr_key_pair_init();
    strptr_t priv_buf = CONST_STR_INIT_NULL, pub_buf = CONST_STR_INIT_NULL;
    if (-1 != cr_gen_keys_buf(1024, &priv_buf, &pub_buf, NULL, keys)) {
        dir = path_add_path(dir, addr, NULL);
        strnadd(&dir, CONST_STR_LEN(".pem"));
        if (0 == save_file(dir->ptr, priv_buf.ptr, priv_buf.len)) {
            str_t *pub_hex = strhex(NULL, pub_buf.ptr, pub_buf.len, 8);
            char *txid = mch_publishfrom(mch, addr, strlen(addr), MCH_STREAM_PUBKEYS, NULL, pub_hex->ptr, pub_hex->len);
            if (txid) {
                printf("%s\n", txid);
                free(txid);
                rc = 0;
            }
            free(pub_hex);
        }
    }
    cr_free_keys(keys);
done:
    cr_done();
    free(dir);
    if (*coin_errmsg) printf("%s\n", coin_errmsg);
    return rc;
}
