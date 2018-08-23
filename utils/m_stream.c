// Publish an RSA key pair
#include <libex/str.h>
#include <libex/file.h>
#include <libsslw/crypt.h>
#include "coin.h"

int main (int argc, const char *argv[]) {
    if (argc != 3) return 1;
    const char *chain = argv[1],
               *addr = argv[2];
    char *txid = NULL;
    if (-1 == cr_init()) return 1;
    mch_load_conf(NULL);
    chain_conf_t *mch = mch_get_conf(chain, strlen(chain));
    if (!mch) goto err;
    if(!(txid = mch_createstream(mch, MCH_STREAM_PUBKEYS, 1))) goto err;
    free(txid);
    if(!(txid = mch_createstream(mch, MCH_STREAM_ITEMS, 1))) goto err;
    free(txid);
    if(!(txid = mch_createstream(mch, MCH_STREAM_ACCESS, 1))) goto err;
    free(txid);
    
    cr_key_pair_t *keys = cr_key_pair_init();
    strptr_t priv = CONST_STR_INIT_NULL, pub = CONST_STR_INIT_NULL;
    if (0 == cr_gen_keys_buf(1024, &priv, &pub, NULL, keys)) {
        str_t *hpub = strhex(NULL, pub.ptr, pub.len, 8);
        save_file("./priv.pem", priv.ptr, priv.len);
        save_file("./pub_hex", hpub->ptr, hpub->len);
        if ((txid = mch_publishfrom(mch, addr, strlen(addr), MCH_STREAM_PUBKEYS, NULL, hpub->ptr, hpub->len))) {
            free(txid);
        }
        free(hpub);
    }
    
    if (priv.ptr) free(priv.ptr);
    if (pub.ptr) free(pub.ptr);
    if (keys)
        cr_free_keys(keys);
    cr_done();
    return 0;
err:
    if (keys) {
        if (keys->msg)
            printf("%s\n", keys->msg);
        cr_free_keys(keys);
    }
    cr_done();
    if (coin_errmsg)
        printf("%d: %s\n", coin_errcode, coin_errmsg);
    return 1;
}