#include "coin.h"

int main (int argc, const char *argv[]) {
    int rc = 1;
    if (argc != 2) return 1;
    const char *chain_name = argv[1];
    char *txid = NULL;
    mch_load_conf(NULL);
    chain_conf_t *mch = mch_get_conf(chain_name, strlen(chain_name));
    if (!mch) return 1;
    printf("create pubkeys stream...");
    if (!(txid = mch_createstream(mch, MCH_STREAM_PUBKEYS, 1))) goto done;
    printf("Ok\n");
    free(txid);
    printf("create items stream...");
    if (!(txid = mch_createstream(mch, MCH_STREAM_ITEMS, 1))) goto done;
    printf("Ok\n");
    free(txid);
    if (!(txid = mch_createstream(mch, MCH_STREAM_ACCESS, 1))) goto done;
    printf("Ok\n");
    rc = 0;
done:
    if (txid) free(txid); else printf("Fail\n");
    if (*coin_errmsg) printf("ERROR: %s\n", coin_errmsg);
    return rc;
}
