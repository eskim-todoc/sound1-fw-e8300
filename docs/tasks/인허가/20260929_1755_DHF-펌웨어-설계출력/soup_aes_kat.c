/* Host-only AES-128 ECB known-answer test, NIST SP 800-38A F.1. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aes.h"

static void decode(const char *hex, uint8_t *bytes, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        unsigned value;
        if (sscanf(hex + 2*i, "%2x", &value) != 1) return;
        bytes[i] = (uint8_t)value;
    }
}

int main(void)
{
    const char *plain[4] = {
        "6bc1bee22e409f96e93d7e117393172a", "ae2d8a571e03ac9c9eb76fac45af8e51",
        "30c81c46a35ce411e5fbc1191a0a52ef", "f69f2445df4f9b17ad2b417be66c3710"};
    const char *cipher[4] = {
        "3ad77bb40d7a3660a89ecaf32466ef97", "f5d3d58503b9699de785895a96fdbaaf",
        "43b1cd7f598ece23881b00e3ed030688", "7b0c785e27e8ad3f8223207104725dd4"};
    uint8_t key[16], input[16], expected[16];
    struct AES_ctx ctx;
    decode("2b7e151628aed2a6abf7158809cf4f3c", key, 16);
    AES_init_ctx(&ctx, key);
    for (unsigned i=0; i<4; ++i) {
        decode(plain[i], input, 16); decode(cipher[i], expected, 16);
        AES_ECB_encrypt(&ctx, input);
        if (memcmp(input, expected, 16)) {printf("FAIL encrypt %u\n", i+1);return 1;}
        decode(cipher[i], input, 16); decode(plain[i], expected, 16);
        AES_ECB_decrypt(&ctx, input);
        if (memcmp(input, expected, 16)) {printf("FAIL decrypt %u\n", i+1);return 1;}
    }
    puts("PASS AES128 ECB: 4 encrypt + 4 decrypt NIST vectors");
    return 0;
}
