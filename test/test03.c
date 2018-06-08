#include "coin.h"

static int fn (json_item_t *ji, void *dummy) {
    if (JSON_STRING == ji->type) {
        char *s = json_str(ji);
        printf("%s\n", s);
        free(s);
    }
    return ENUM_CONTINUE;
}

int main () {
    curl_global_init(CURL_GLOBAL_NOTHING);
    mch_load_conf(NULL);
    chain_conf_t *mch = mch_get_conf(CONST_STR_LEN("uulala"));
    if (mch) {
        int rc;
        rc = mch_getaddresses(mch, fn, NULL, 0);
        rc = mch_dumpwallet(mch, "/tmp/uulala.dump");
        rc = mch_importwallet(mch, "/tmp/uulala.dump");
        rc = mch_getaddresses(mch, fn, NULL, 0);
    }
    curl_global_cleanup();
    return 0;
}

