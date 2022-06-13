#include "aes.h"
#include <stdlib.h>
#include <string.h>

// Round constants - 0th element, isn't on the wiki I just put it there to pad the buffer
static const uint8_t k_round_constants[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};

// Used when doing the byte substitution, you have a byte toy want to swap and you can use it as an index into
// this table to get the substitution
static const uint8_t k_sub_box[] = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82,
  0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
  0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96,
  0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
  0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb,
  0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
  0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff,
  0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32,
  0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
  0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6,
  0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
  0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e,
  0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
  0xb0, 0x54, 0xbb, 0x16};

void word_shift_left(uint8_t* i)
{
    // We recieve four bytes making up a word, we are going to left shift them, meaning that the most
    // significant byte in the word goes round to the end and everything else gets shifted one place up. But
    // which one is the most significant? for now I think the answer is probably the first one

    uint8_t msb = *i;
    i[0] = i[1];
    i[1] = i[2];
    i[2] = i[3];
    i[3] = msb;
}

void sub_byte(uint8_t* i) { *i = k_sub_box[*i]; }

void sub_word(uint8_t* w)
{
    for (uint32_t i = 0; i < 4; i++) {
        sub_byte(w + i);
    }
}

void xor_word(uint8_t* out, uint8_t* a, uint8_t* b)
{
    for (uint32_t i = 0; i < 4; i++) {
        out[i] = a[i] ^ b[i];
    }
}

aes_enum aes_init_round_keys(aes_working_buffer* buff)
{
    // First validate that the key is the right length, it can only be 128, 192, 256 bits large, but remember
    // users will be using bytes
    buff->round_count = aes_helper_round_count(buff->encryption_key_size);
    if (buff->round_count == 0) return e_aes_incorrect_key_size;

    // Going from https://en.wikipedia.org/wiki/AES_key_schedule. the key schedule is a way of generating a
    // 128bit round sized round keys for each round from one varying length key

    // Lets get this working for a fixed key size of 128 first!
    // As the key expansion or key scheduling gets pretty confusing
    if (buff->encryption_key_size != 128 / 8)
        return e_aes_incorrect_key_size;  // TODO figure out other key sizes

    // How many words make up the base encryption key. From this encryption key we need to generate 4N words
    const uint8_t N = buff->encryption_key_size / sizeof(uint32_t);

    // We know that the buffers holding the round key are allocated next to each other, this is probably just
    // the worst approach, but taking a pointer to the first member should be able to cross the bounds between
    // the elements without corrupting the stack
    uint8_t* w0 = buff->round_keys->x;

    // The first N words are the base encryption key
    memcpy(w0, buff->encryption_key, buff->encryption_key_size);

    // The next 3 N words follow a set pattern
    for (uint32_t i = N; i < 4 * N; i++) {
        // We almost always look back so get the ith word
        uint8_t* wI = w0 + i;

        if (i % N == 0) {
            // w_i = w_{i-N} XOR subWord(RotWord(w_{i-1})) XOR RoundConstant
            // However the round constant only has content in the first byte the rest are all 0s
            // So we just need to xor the first byte at w_i
            uint8_t sub[4];
            memcpy(sub, wI - 1, 4 * sizeof(uint8_t));
            word_shift_left(sub);
            sub_word(sub);
            xor_word(wI, wI - N, sub);

            // We know i / N nicely because of the above if statement and will be greater than 1
            uint8_t round_constant = k_round_constants[i / N];
            wI[0] = wI[0] ^ round_constant;

        } else if (N > 6 && (i % N == 4)) {
            // w_i - W_{i-N} XOR SubWord(W_{i-1})
            uint8_t sub[4];
            memcpy(sub, wI - 1, 4 * sizeof(uint8_t));
            sub_word(sub);
            xor_word(wI, wI - N, sub);
        } else {
            // w_i = W_{i-N} XOR W_{i-1}
            xor_word(wI, wI - N, wI - 1);
        }
    }

    return e_aes_success;
}

uint8_t aes_helper_round_count(uint32_t key_length)
{
    switch (key_length) {
        case (128 / 8):
            return 11;
        case (192 / 8):
            return 13;
        case (256 / 8):
            return 15;
        default:
            return 0;
    }
}
