#include "coin.h"
#include "ether.h"

int main () {
    str_t *s = coin_str_to_int_str("12.123");
    printf("%s\n", s->ptr);
    free(s);
    s = coin_str_to_int_str("12");
    printf("%s\n", s->ptr);
    free(s);
    s = coin_str_to_int_str("12.00");
    printf("%s\n", s->ptr);
    free(s);
    s = coin_str_to_int_str("0.123");
    printf("%s\n", s->ptr);
    free(s);
    s = coin_str_to_int_str("00.123");
    printf("%s\n", s->ptr);
    free(s);
    return 0;
}
