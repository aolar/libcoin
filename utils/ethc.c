#include <libex/array.h>
#include <libex/file.h>
#include "ether.h"
#include "linenoise.h"
#if 0
int mint (const char *to, const char *amount_str) {
    if (!to) {
        dprintf(STDERR_FILENO, "destination address expected\n");
        return 1;
    }
    if (!amount_str) {
        dprintf(STDERR_FILENO, "amount expected\n");
        return 1;
    }

    str_t *contract_def = load_all_file(contract_file, 64, 65535);
    if (contract_def) {
        int rc = -1;
        eth_contract_t *contract = eth_load_contract(contract_addr, contract_def->ptr, contract_def->len);
        eth_ctx_t ctx;
        int unlocked = eth_unlock_account(auth_addr, pass);
        switch (unlocked) {
            case -1:
                dprintf(STDERR_FILENO, "%d: %s\n", eth_errcode, eth_errmsg);
                goto done;
            case 0:
                dprintf(STDERR_FILENO, "illegal password\n");
                goto done;
            default: break;
        }
        mpz_t amount;
        mpz_init_set_str(amount, amount_str, 10);
        memset(&ctx, 0, sizeof(eth_ctx_t));
        if (-1 == eth_prepare_exec(contract, from_addr, "mint(address,uint256)", &ctx))
            goto done;
        eth_param_address(&ctx, to);
        eth_param_int(&ctx, amount);
        if (-1 == (rc = eth_exec(&ctx, ETH_NULL, NULL, ETH_NULL)))
            dprintf(STDERR_FILENO, "%d: %s\n", eth_errcode, eth_errmsg);
        else
            dprintf(STDOUT_FILENO, "%s\n", ctx.result->ptr);
done:
        eth_ctx_clear(&ctx);
        eth_free_contract(contract);
        free(contract_def);
        return -rc;
    }
    return 1;
}
#endif
static void linenoise_ac (char *buf, int *posp, int len, size_t cols) {
    static const char *cstr = "quit";
    if (len > 0 && len < 4 && *posp > 0) {
        for (int i = 0; i < *posp; ++i)
            if (buf[i] != cstr[i]) return;
        strcpy(buf, "quit");
        *posp = strlen(buf);
    }
}

int main (int argc, const char *argv[]) {
    char *line;
    curl_global_init(CURL_GLOBAL_ALL);
    linenoiseHistoryLoadFile("history.txt");
    linenoiseACHook = linenoise_ac;
    while ((line = linenoise("> "))) {
        if (*line) {
            char *ptr = line;
            strptr_t tok;
            size_t len;
            if (!strcmp(line, "quit"))
                break;
            len = strlen(line);
            if (0 == strntok(&ptr, &len, CONST_STR_LEN(STR_SPACES), &tok)) {
                if (0 == cmpstr(tok.ptr, tok.len, CONST_STR_LEN("new_account"))) {
                    if (0 == strntok(&ptr, &len, CONST_STR_LEN(STR_SPACES), &tok)) {
                        char address [65], *pass = strndup(tok.ptr, tok.len);
                        if (eth_new_account(pass, address, sizeof(address)))
                            printf("%s\n", address);
                        else
                            printf("%d: %s\n", coin_errcode, coin_errmsg);
                        free(pass);
                    }
                }
            }
            linenoiseHistoryAdd(line);
        }
    }
    linenoiseHistorySaveFile("history.txt");
    curl_global_cleanup();
    return 0;
}
