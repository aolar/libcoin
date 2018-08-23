// Store some data
#include <libex/str.h>
#include <libex/file.h>
#include "coin.h"

int main (int argc, const char *argv[]) {
    if (argc != 3) return 1;
    const char *chain = argv[1],
               *addr = argv[2],
               *fname = argv[3]
    char *txid = NULL;
    str_t *data = load_all_file(fname, 8, 1024*1024);
          *hdata;
    chain_conf_t *mch;
    if (!data) return 1;
    hdata = strhex(NULL, data->ptr, data->len, 8);
    free(data);
    if (-1 == cr_init()) return 1;
    mch_load_conf(NULL);
    if (!(mch = mch_get_conf(chain, strlen(chain)))) goto err;
    if (!(data = 
    if ((txid = mch_publishfrom(mch, addr, strlen(addr), MCH_STREAM_ITEMS, fname, hdata->ptr, hdata->len))) {
        printf("%s\n", txid);
        free(txid);
    }
    free(hdata);
    return 0;
err:
    free(hdata);
    cr_done();
    return 1;
}
