//
//  BBREthereumAddress.c
//  WalletKitCore Ethereum
//
//  Created by Ed Gamble on 2/21/2018.
//  Copyright © 2018-2019 Breadwinner AG.  All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "support/BRCrypto.h"
#include "support/BRKey.h"
#include "support/BRBIP32Sequence.h"
#include "support/BRBIP39Mnemonic.h"
#include "support/BRBase58.h"
#include "support/BRBIP39WordsEn.h"
#include "ethereum/base/BREthereumBase.h"
#include "BREthereumAccount.h"

#if defined(DEBUG)
#define DEBUG_PAPER_KEY  "boring head harsh green empty clip fatal typical found crane dinner timber"
#endif

// BIP39 test vectors
// https://github.com/trezor/python-mnemonic/blob/master/vectors.json

// Ethereum
// https://mytokenwallet.com/bip39.html

#define PRIMARY_ADDRESS_BIP44_INDEX 0

/* Forward Declarations */
//static BREthereumEncodedAddress
//accountCreateAddress (BREthereumAccount account, UInt512 seed, uint32_t index);

//
// Locale-Based BIP-39 Word List
//
static const char **sharedWordList;

#define WORD_LIST_LENGTH 2048

extern int
installSharedWordList (const char *wordList[], int wordListLength) {
    if (BIP39_WORDLIST_COUNT != wordListLength)
        return 0;
    
    sharedWordList = wordList;
    
    return 1;
}

// Address Detail

/**
 * An EthereumAddress is as '0x'-prefixed, hex-encoded string with an overall lenght of 42
 * characters.  Addresses can be explicitly provided - such as with a 'send to' addresses; or can
 * be derived using BIP44 scheme - such as with internal addresses.
 */
typedef struct BREthereumAddressDetailRecord {
    BREthereumAddress raw;
    
    /**
     * The 'official' ethereum address string for (the external representation of) this
     * BREthereum address.
     *
     * THIS IS NOT A SIMPLE STRING; this is a hex encoded (with hexEncode) string prefixed with
     * "0x".  Generally, when using this string, for example when RLP encoding, one needs to
     * convert back to the byte array (use rlpEncodeItemHexString())
     */
    char string[43];    // '0x' + <40 chars> + '\0'
    
    /**
     * The public key.  This started out as a BIP44 264 bits (65 bytes) array with a value of
     * 0x04 at byte 0; we strip off that first byte and are left with 64.  Go figure.
     */
    uint8_t publicKey [64];  // BIP44: 'Master Public Key 'M' (264 bits) - 8
    
    /**
     * The BIP-44 Index used for this key.
     */
    uint32_t index;
    
    /**
     * The NEXT nonce value
     */
    uint64_t nonce;
} BREthereumAddressDetail;

static BRKey // 65 bytes
addressDetailGetPublicKey (BREthereumAddressDetail *address) {
    BRKey result;
    BRKeyClean(&result);
    
    result.pubKey[0] = 0x04;
    memcpy (&result.pubKey[1], address->publicKey, sizeof (address->publicKey));
    
    return result;
}

static void
addressDetailFillKey(BREthereumAddressDetail *address, const BRKey *key, uint32_t index) {
    
    address->nonce = 0;
    address->index = index;
    
    // Seriously???
    //
    // https://kobl.one/blog/create-full-ethereum-keypair-and-address/#derive-the-ethereum-address-from-the-public-key
    //
    // "The public key is what we need in order to derive its Ethereum address. Every EC public key
    // begins with the 0x04 prefix before giving the location of the two point on the curve. You
    // should remove this leading 0x04 byte in order to hash it correctly. ...
    
    assert (key->pubKey[0] == 0x04);
    
    // Strip off byte 0
    memcpy(address->publicKey, &key->pubKey[1], sizeof (address->publicKey));
    
    
    address->raw = ethAddressCreateKey(key);
    char *string = ethAddressGetEncodedString(address->raw, 1);
    memcpy (address->string, string, 42);
    address->string[42] = '\0';
    free (string);
}

static void
addressDetailFillSeed (BREthereumAddressDetail *address, UInt512 seed, uint32_t index) {
    BRKey key = ethAccountDerivePrivateKeyFromSeed (seed, index);
    
    // Seriously???
    //
    // https://kobl.one/blog/create-full-ethereum-keypair-and-address/#derive-the-ethereum-address-from-the-public-key
    //
    // "The private key must be 32 bytes and not begin with 0x00 and the public one must be
    // uncompressed and 64 bytes long or 65 with the constant 0x04 prefix. More on that in the
    // next section. ...

    size_t keyLen = BRKeyPubKey(&key, NULL, 0);
    assert (65 == keyLen);
    
    // "The public key is what we need in order to derive its Ethereum address. Every EC public key
    // begins with the 0x04 prefix before giving the location of the two point on the curve. You
    // should remove this leading 0x04 byte in order to hash it correctly. ...
    addressDetailFillKey(address, &key, index);
}

#if defined (DEBUG)
static void
addressDetailFillRaw (BREthereumAddressDetail *address, const char *string) {
    address->index = 0;
    address->nonce = 0;
    strlcpy (&address->string[0], string, 43);
    address->raw = ethAddressCreate(string);
}
#endif

// BRSet HashValue, HashEqual


//
// Account
//
struct BREthereumAccountRecord {

    /**
     * The master public key derived from the seed as per BIP-32
     */
    BRMasterPubKey masterPubKey;
    
    /**
     * The primary address for this account - aka address[0].
     */
    BREthereumAddressDetail primaryAddress;
};

extern BREthereumAccount
ethAccountCreateWithBIP32Seed (UInt512 seed) {
    BREthereumAccount account = (BREthereumAccount) calloc (1, sizeof (struct BREthereumAccountRecord));
    
    // Assign the key; create the primary address.
    account->masterPubKey = BRBIP32MasterPubKey(&seed, sizeof(seed));
    addressDetailFillSeed(&account->primaryAddress, seed, PRIMARY_ADDRESS_BIP44_INDEX);
    
    return account;
    
}

extern BREthereumAccount
ethAccountCreateWithPublicKey (const BRKey key) { // 65 bytes, 0x04-prefixed, uncompressed public key
    BREthereumAccount account = (BREthereumAccount) calloc (1, sizeof (struct BREthereumAccountRecord));
    
    // Assign the key; create the primary address.
    account->masterPubKey = BR_MASTER_PUBKEY_NONE;
    addressDetailFillKey(&account->primaryAddress, &key, PRIMARY_ADDRESS_BIP44_INDEX);
    
    return account;
}

extern BREthereumAccount
ethAccountCreateDetailed(const char *paperKey, const char *wordList[], const int wordListLength) {
    
    // Validate arguments
    if (NULL == paperKey || NULL == wordList || BIP39_WORDLIST_COUNT != wordListLength)
        return NULL;
    
    // Validate paperKey
    if (0 == BRBIP39Decode(NULL, 0, wordList, paperKey))
#if ! defined (DEBUG)
        return NULL;
#else
    {
        // For a debug build, with an invalid paperKey, see if paperKey is actually a public address
        if (NULL == paperKey || 42 != strlen (paperKey) || 0 != strncmp (paperKey, "0x", 2)) return NULL;

        // Use a well-known public paper key, but overwrite the address.  This won't work for
        // signing, but will work for viewing.
        BREthereumAccount account = ethAccountCreateWithBIP32Seed(ethAccountDeriveSeedFromPaperKey(DEBUG_PAPER_KEY));
        addressDetailFillRaw (&account->primaryAddress, paperKey);
        return account;
    }
#endif
    // Generate the 512bit private key using a BIP39 paperKey
    return ethAccountCreateWithBIP32Seed(ethAccountDeriveSeedFromPaperKey(paperKey));
}

extern BREthereumAccount
ethAccountCreate (const char *paperKey) {
    if (NULL == sharedWordList)
        installSharedWordList(BRBIP39WordsEn, BIP39_WORDLIST_COUNT);
    
    return ethAccountCreateDetailed(paperKey, sharedWordList, BIP39_WORDLIST_COUNT);
}

extern void
ethAccountRelease (BREthereumAccount account) {
    free (account);
}

extern BREthereumAddress
ethAccountGetPrimaryAddress(BREthereumAccount account) {
    return account->primaryAddress.raw;
}

extern char *
ethAccountGetPrimaryAddressString (BREthereumAccount account) {
    return strdup(account->primaryAddress.string);
    
}

extern BRKey
ethAccountGetPrimaryAddressPublicKey (BREthereumAccount account) {
    return addressDetailGetPublicKey(&account->primaryAddress);
}

extern BRKey
ethAccountGetPrimaryAddressPrivateKey (BREthereumAccount account,
                                       const char *paperKey) {
    return ethAccountDerivePrivateKeyFromSeed(ethAccountDeriveSeedFromPaperKey(paperKey),
                                    account->primaryAddress.index);
}

#if defined (DEBUG)
extern const char *
ethAccountGetPrimaryAddressPublicKeyString (BREthereumAccount account, int compressed) {
    // The byte array at address->publicKey has the '04' 'uncompressed' prefix removed.  Thus
    // the value in publicKey is uncompressed and 64 bytes.  As a string, this result will have
    // an 0x0<n> prefix where 'n' is in { 4: uncompressed, 2: compressed even, 3: compressed odd }.
    
    // Default, uncompressed
    char *prefix = "0x04";
    size_t sourceLen = sizeof (account->primaryAddress.publicKey);           // 64 bytes: { x y }
    
    if (compressed) {
        sourceLen /= 2;  // use 'x'; skip 'y'
        prefix = (0 == account->primaryAddress.publicKey[63] % 2 ? "0x02" : "0x03");
    }
    
    char *result = malloc (4 + 2 * sourceLen + 1);
    strcpy (result, prefix);  // encode properly...
    hexEncode(&result[4], 2 * sourceLen + 1, account->primaryAddress.publicKey, sourceLen);
    
    return result;
}
#endif

extern BREthereumBoolean
ethAccountHasAddress(BREthereumAccount account,
                     BREthereumAddress address) {
    return ethAddressEqual(account->primaryAddress.raw, address);
}

extern BREthereumSignature
ethAccountSignBytesWithPrivateKey(BREthereumAccount account,
                                  BREthereumAddress address,
                                  BREthereumSignatureType type,
                                  uint8_t *bytes,
                                  size_t bytesCount,
                                  BRKey privateKey) {
    return ethSignatureCreate(type, bytes, bytesCount, privateKey);
}

extern BREthereumSignature
ethAccountSignBytes(BREthereumAccount account,
                    BREthereumAddress address,
                    BREthereumSignatureType type,
                    uint8_t *bytes,
                    size_t bytesCount,
                    const char *paperKey) {
    UInt512 seed = ethAccountDeriveSeedFromPaperKey(paperKey);
    // TODO: Account has Address
    return ethAccountSignBytesWithPrivateKey(account,
                                             address,
                                             type,
                                             bytes,
                                             bytesCount,
                                             ethAccountDerivePrivateKeyFromSeed(seed, account->primaryAddress.index));
}

//
// Support
//

extern UInt512
ethAccountDeriveSeedFromPaperKey (const char *paperKey) {
    // Generate the 512bit private key using a BIP39 paperKey
    UInt512 seed = UINT512_ZERO;
    BRBIP39DeriveKey(seed.u8, paperKey, NULL); // no passphrase
    return seed;
}

extern BRKey
ethAccountDerivePrivateKeyFromSeed (UInt512 seed, uint32_t index) {
    BRKey privateKey;
    
    // The BIP32 privateKey for m/44'/60'/0'/0/index
    BRBIP32PrivKeyPath(&privateKey, &seed, sizeof(UInt512), 5, (const uint32_t []){
                       44 | BIP32_HARD,          // purpose  : BIP-44
                       60 | BIP32_HARD,          // coin_type: Ethereum
                       0 | BIP32_HARD,          // account  : <n/a>
                       0,                        // change   : not change
                       index });                 // index    :
    
    privateKey.compressed = 0;
    
    return privateKey;
}

//
// New
//
extern uint32_t
ethAccountGetAddressIndex (BREthereumAccount account,
                           BREthereumAddress address) {
    // TODO: Lookup address, assert address
    return account->primaryAddress.index;
}

extern uint64_t
ethAccountGetAddressNonce (BREthereumAccount account,
                           BREthereumAddress address) {
    // TODO: Lookup address, assert address
    return account->primaryAddress.nonce;
    
}

private_extern void
ethAccountSetAddressNonce(BREthereumAccount account,
                          BREthereumAddress address,
                          uint64_t nonce,
                          BREthereumBoolean force) {
    // TODO: Lookup address, assert address
    if (ETHEREUM_BOOLEAN_IS_TRUE(force) || nonce > account->primaryAddress.nonce)
        account->primaryAddress.nonce = nonce;
}

private_extern uint64_t
ethAccountGetThenIncrementAddressNonce(BREthereumAccount account,
                                       BREthereumAddress address) {
    // TODO: Lookup address, assert address
    return account->primaryAddress.nonce++;
}
