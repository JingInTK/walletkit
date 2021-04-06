//
//  BRCryptoFileServiceClient.h
//  BRCrypto
//
//  Created by Bijan Soleymani on 4/6/21.
//  Copyright © 2021 breadwallet. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#ifndef BRCryptoFileServiceClient_h
#define BRCryptoFileServiceClient_h

#include "BRCryptoBase.h"
// ...

#ifdef __cplusplus
extern "C" {
#endif

// An arbitary pointer to hold client state
typedef void *BRCryptoFileServiceClientContext;

// ... stuff the client must provide

typedef struct BRCryptoFileServiceClientRecord *BRCryptoFileServiceClient;

extern BRCryptoFileServiceClient
cryptoFileServiceClientCreate (BRCryptoFileServiceClientContext context /* , stuff the client must provide */);

// The default SQLite implementation; create a DB under `path`
extern BRCryptoFileServiceClient
cryptoFileServiceClientCreateDefault (const char *path /* , ... */);

// Create a client that saves and restores nothing
extern BRCryptoFileServiceClient
cryptoFileServiceClientCreateNOP (void);

#ifdef __cplusplus
}
#endif

#endif /* BRCryptoFileServiceClient_h */
