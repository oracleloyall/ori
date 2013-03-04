/*
 * Copyright (c) 2013 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <string>
#include <exception>
#include <stdexcept>

#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/oricrypt.h>

#ifdef OPENSSL_NO_AES
#error "OpenSSL AES support not present!"
#endif
#if defined(OPENSSL_NO_SHA) || defined(OPENSSL_NO_SHA1)
#error "OpenSSL SHA support not present!"
#endif

using namespace std;

/*
 * Encrypts the plaintext given a key and a randomly generated salt. The salt 
 * is prepended to the ciphertext that is returned.
 */
string
OriCrypt_Encrypt(const string &plaintext, const string &key)
{
    int bits, clen, flen;
    unsigned char salt[PKCS5_SALT_LEN];
    unsigned char keyData[32];
    unsigned char ivData[32];
    unsigned char *buf;
    EVP_CIPHER_CTX ctx;
    string ciphertext;

    // Generate random salt and prepend it
    // XXX: PKCS#5 implementation in OpenSSL uses RAND_pseudo_bytes
    RAND_pseudo_bytes(salt, PKCS5_SALT_LEN);
    ciphertext.assign((const char *)&salt, PKCS5_SALT_LEN);

    bits = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha256(), salt,
                          (const unsigned char *)key.c_str(), key.size(),
                          /* nrounds */5, keyData, ivData);
    if (bits != 32) {
        NOT_IMPLEMENTED(false);
    };

    clen = plaintext.size() + AES_BLOCK_SIZE;
    buf = new unsigned char[clen];

    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, keyData, ivData);
    EVP_EncryptUpdate(&ctx, buf, &clen,
                      (const unsigned char *)plaintext.data(),
                      plaintext.size());
    EVP_EncryptFinal_ex(&ctx, buf+clen, &flen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    ciphertext.append((const char *)buf, clen+flen);
    delete buf;

    return ciphertext;
}

/*
 * Decrypted the ciphertext given a key and extracts the salt from the 
 * ciphertext blob.
 */
string
OriCrypt_Decrypt(const string &ciphertext, const string &key)
{
    int bits, plen, flen;
    unsigned char salt[8];
    unsigned char keyData[32];
    unsigned char ivData[32];
    unsigned char *buf;
    EVP_CIPHER_CTX ctx;
    string plaintext;

    if (ciphertext.size() <= PKCS5_SALT_LEN)
        throw length_error("OriCrypt_Decrypt");

    memcpy(salt, ciphertext.data(), PKCS5_SALT_LEN);

    bits = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha256(), salt,
                          (const unsigned char *)key.c_str(), key.size(),
                          /* nrounds */5, keyData, ivData);
    if (bits != 32) {
        NOT_IMPLEMENTED(false);
    };

    plen = ciphertext.size() + AES_BLOCK_SIZE;
    buf = new unsigned char[plen];

    EVP_CIPHER_CTX_init(&ctx);
    EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, keyData, ivData);
    EVP_DecryptUpdate(&ctx, buf, &plen,
                      (const unsigned char *)ciphertext.data() + PKCS5_SALT_LEN,
                      ciphertext.size() - PKCS5_SALT_LEN);
    EVP_DecryptFinal_ex(&ctx, buf+plen, &flen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    plaintext.assign((const char *)buf, plen+flen);
    delete buf;

    return plaintext;
}

int
OriCrypt_selfTest()
{
    int i = 0;
    string key = "secret key";
    string p, c;
    string tests[] = {
        "short",
        "Another test string",
        "This is a longer test string that should span an AES block.",
        "",
    };

    cout << "Testing OriCrypt ..." << endl;

    while (tests[i] != "") {
        c = OriCrypt_Encrypt(tests[i], key);
        p = OriCrypt_Decrypt(c, key);
        if (p != tests[i]) {
            cout << "Error decrypted ciphertext does not match original!" << endl;
            return -1;
        }
        i++;
    }

    return 0;
}

