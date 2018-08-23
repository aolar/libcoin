#include <libex/str.h>
#include <libex/file.h>
#include <libsslw/crypt.h>
#include "coin.h"

typedef struct {
    char txid [512];
    int vout;
} addr_vout_t;

static int on_find (json_item_t *ji, void *data) {
    addr_vout_t *av = (addr_vout_t*)data;
    json_item_t *j_txid = json_find(ji->data.o, CONST_STR_LEN("txid"), JSON_STRING),
                *j_vout = json_find(ji->data.o, CONST_STR_LEN("vout"), JSON_INTEGER);
    av->vout = j_vout->data.i;
    strncpy(av->txid, j_txid->data.s.ptr, j_txid->data.s.len);
    return ENUM_BREAK;
}

int main (int argc, const char *argv[]) {
    if (argc != 2) return 1;
    const char *chain = argv[1],
               *addr = argv[2];
    mch_load_conf(NULL);
    chain_conf_t *mch = mch_get_conf(NULL);
    if (!mch) return 1;
    addr_vout_t av;
    memset(&av, 0, sizeof av);
    if (0 != mch_liststreampublisheritems(mch, MCH_STREAM_PUBKEYS, addr, strlen(addr), 0, 1, on_find, &av, ENUM_STOP_IF_BREAK)) {
        
    }
    return 0;
}
