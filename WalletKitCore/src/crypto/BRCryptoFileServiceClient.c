//
//  BRCryptoFileServiceClient.c
//  BRCrypto
//
//  Created by Bijan Soleymani on 4/6/21.
//  Copyright © 2021 breadwallet. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#include "BRCryptoFileServiceClientP.h"

// ...

extern BRCryptoFileServiceClient
cryptoFileServiceClientCreate (BRCryptoFileServiceClientContext context /* , ... */) {
    return NULL;
}

// The default SQLite implementation
extern BRCryptoFileServiceClient
cryptoFileServiceClientCreateDefault (const char *path) {
    return NULL;
}

// Create a client that saves and restores nothing
extern BRCryptoFileServiceClient
cryptoFileServiceClientCreateNOP (void) {
    return NULL;
}

