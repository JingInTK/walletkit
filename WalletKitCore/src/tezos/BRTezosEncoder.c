//
//  BRTezosEncoder.c
//  WalletKitCore
//
//  Created by Ehsan Rezaie on 2020-07-22.
//  Copyright © 2020 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//

#include <stdint.h>
#include "BRTezosEncoder.h"
#include "BRTezosAddress.h"
#include "BRTezosTransaction.h"
#include "support/util/BRUtilMath.h"
#include "support/BRBase58.h"
#include <stdio.h>
#include <assert.h>


static WKData
encodeAddress (BRTezosAddress address) {
    // address bytes with 3-byte TZx prefix replaced with a 1-byte prefix
    
    assert (address);
    assert (tezosAddressIsImplicit (address));
    
    size_t len = tezosAddressGetRawSize (address);
    uint8_t bytes[len];
    tezosAddressGetRawBytes (address, bytes, len);
    
    uint8_t prefix;
    
    if (0 == memcmp(bytes, TZ1_PREFIX, sizeof(TZ1_PREFIX)))
        prefix = 0x00;
    else if (0 == memcmp(bytes, TZ2_PREFIX, sizeof(TZ2_PREFIX)))
        prefix = 0x01;
    else if (0 == memcmp(bytes, TZ3_PREFIX, sizeof(TZ1_PREFIX)))
        prefix = 0x02;
    else
        assert(0);
    
    uint8_t encoded[len-2];
    memcpy(&encoded[0], &prefix, 1);
    memcpy(&encoded[1], &bytes[3], len-3);
    
    return wkDataCopy (encoded, sizeof(encoded));
}

static WKData
encodePublicKey (uint8_t * pubKey) {
    uint8_t prefix = 0x0; // ed25519
    WKData encoded = wkDataNew(TEZOS_PUBLIC_KEY_SIZE + 1);
    memcpy(encoded.bytes, &prefix, 1);
    memcpy(&encoded.bytes[1], pubKey, TEZOS_PUBLIC_KEY_SIZE);
    return encoded;
}

extern WKData
encodeZarith (int64_t value) {
    assert (value >= 0);
    uint8_t result[32] = {0};
    size_t resultSize = 0;
    
    uint64_t input = (uint64_t)value;
    while (input >= 0x80) {
        result[resultSize++] = (uint8_t)((input & 0xff) | 0x80);
        input >>= 7;
    }
    result[resultSize++] = (uint8_t)input;
    
    return wkDataCopy(&result[0], resultSize);
}

static WKData
encodeBool (bool value) {
    WKData encoded = wkDataNew(1);
    encoded.bytes[0] = value ? 0xff : 0x00;
    return encoded;
}

static WKData
encodeOperationKind (BRTezosOperationKind kind) {
    uint8_t bytes[1] = { kind };
    return wkDataCopy (bytes, 1);
}

static WKData
encodeBranch (BRTezosHash blockHash) {
    // omit prefix
    size_t numPrefixBytes = 2;
    size_t branchSize = sizeof(blockHash.bytes) - numPrefixBytes;
    
    WKData branchData = wkDataNew(branchSize);
    memcpy(branchData.bytes, &blockHash.bytes[numPrefixBytes], branchSize);
    
    return branchData;
}

extern WKData
tezosSerializeTransaction (BRTezosTransaction tx) {
    assert (tx);
    
    BRTezosOperationData opData = tezosTransactionGetOperationData(tx);
    
    BRTezosAddress source = tezosTransactionGetSource(tx);
    BRTezosFeeBasis feeBasis = tezosTransactionGetFeeBasis(tx);
    int64_t gasLimit = tezosFeeBasisGetGasLimit(feeBasis);
    int64_t storageLimit = tezosFeeBasisGetStorageLimit(feeBasis);
    
    size_t maxFields = 10;
    WKData fields[maxFields];
    size_t numFields = 0;
    fields[numFields++] = encodeOperationKind(opData.kind);
    fields[numFields++] = encodeAddress(source);
    fields[numFields++] = encodeZarith(tezosTransactionGetFee(tx));
    fields[numFields++] = encodeZarith(tezosTransactionGetCounter(tx));
    fields[numFields++] = encodeZarith(gasLimit);
    fields[numFields++] = encodeZarith(storageLimit);
    
    if (TEZOS_OP_TRANSACTION == opData.kind) {
        fields[numFields++] = encodeZarith(opData.u.transaction.amount);
        //TODO:XTZ support sending to KT addresses
        fields[numFields++] = encodeBool(false); // is originated (KT) address
        fields[numFields++] = encodeAddress(opData.u.transaction.target);
        fields[numFields++] = encodeBool(false); // contract execution params (0x0 for no params)
    } else if (TEZOS_OP_DELEGATION == opData.kind) {
        if (NULL != opData.u.delegation.target) {
            fields[numFields++] = encodeBool(true); // set delegate
            fields[numFields++] = encodeAddress(opData.u.delegation.target);
        } else {
            fields[numFields++] = encodeBool(false); // remove delegate
        }
    } else if (TEZOS_OP_REVEAL == opData.kind) {
        fields[numFields++] = encodePublicKey(opData.u.reveal.publicKey);
    } else {
        // unsupported
        assert(0);
    }
    
    WKData serialized = wkDataConcat(fields, numFields);
    
    tezosAddressFree (source);
    tezosTransactionFreeOperationData(opData);
    for (int i=0; i < numFields; i++) {
        wkDataFree(fields[i]);
    }
    
    return serialized;
}

extern WKData
tezosSerializeOperationList (BRTezosTransaction * tx, size_t txCount, BRTezosHash blockHash) {
    
    WKData fields[txCount + 1];
    size_t numFields = 0;
    
    // operation list = branch + [reveal op bytes] + transaction/delegation op bytes
    
    fields[numFields++] = encodeBranch(blockHash);
    
    for (int i=0; i < txCount; i++) {
        fields[numFields++] = tezosSerializeTransaction(tx[i]);
    }
    
    WKData serialized = wkDataConcat(fields, numFields);
    
    for (int i=0; i < numFields; i++) {
        wkDataFree(fields[i]);
    }
    
    return serialized;
}
