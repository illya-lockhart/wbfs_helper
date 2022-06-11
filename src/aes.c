#include "aes.h"

void word_shift_left(uint32_t* i)
{
    // The left shift is done in bytes, it switches the most significant byte back round to the beginning and
    // then every byte gets shifted up a place. This is performed on a 32bit int as that's the size of a word
    *i = ((0xFF000000 & *i) >> 24) | ((0x00FF0000 & *i) << 8) | ((0x0000FF00 & *i) << 8) |
         ((0x000000FF & *i) << 8);
}

aes_enum aes_init_round_keys(aes_working_buffer* buff)
{
    // First validate that the key is the right length, it can only be 128, 192, 256 bits large, but remember
    // users will be using bytes
    buff->round_count = aes_helper_round_count(buff->encryption_key_size);
    if (buff->round_count == 0) return e_aes_incorrect_key_size;

    return e_aes_success;
}

uint8_t aes_helper_round_count(uint32_t key_length)
{
    switch (key_length) {
        case (128 / 8):
            return 10;
        case (192 / 8):
            return 12;
        case (256 / 8):
            return 14;
        default:
            return 0;
    }
}
