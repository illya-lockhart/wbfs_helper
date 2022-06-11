/**
 * AES 128 CBC encryption and decryption helper library.
 * There is multiple rounds that happen during the encryption. Each round uses a different key refered to as a
 * "round key" The number of rounds depends on the length of the encryption key used.
 *
 * Author  : Illya L
 * License : None
 */
#ifndef __AES_ENCRYPT_DECRYPT_HEADER_H__
#define __AES_ENCRYPT_DECRYPT_HEADER_H__
#include <stdint.h>

/*************************************************************************************************************
 * Structure definitions
 *************************************************************************************************************/

typedef struct aes_128_key {
    uint8_t x[16];
} aes_128_key;

/**
 * Alloctae all the resources that is needed in order for the aes algorithm to decrypt or encrypt the data.
 * From the private key we have to generate a 128 bit round key for every round, and then one more extra round
 * key. Since the round count changes based on the length of the encryption key we're going to allocate for
 * the largest possible which is 14 rounds
 */
typedef struct aes_working_buffer {
    uint8_t encryption_key[256 / 8];
    uint8_t encryption_key_size;
    uint8_t round_count;
    aes_128_key round_keys[15];
    aes_128_key init_vector;

} aes_working_buffer;

/*************************************************************************************************************
 * Enums for return codes
 *************************************************************************************************************/

typedef enum aes_enum { e_aes_success, e_aes_incorrect_key_size } aes_enum;

/*************************************************************************************************************
 * Functions that do a large portion of the work. None of these functions should ever allocate memory, this is
 * left up to the user to do.
 *
 * Most of these functions should return an enum specifying a sucess code
 *************************************************************************************************************/

aes_enum aes_init_round_keys(aes_working_buffer *buff);

/*************************************************************************************************************
 * Helper functions that don't have a return type, do something simple
 *************************************************************************************************************/

/**
 * @brief Tells the user how rounds there is going to be given their key length, this is useful as they're
 * going to responable for allocating space for the round keys
 * @returns How many rounds take place
 * @param key_length The length of the key in bytes
 */
uint8_t aes_helper_round_count(uint32_t key_length);

#endif  // !__AES_ENCRYPT_DECRYPT_HEADER_H__
