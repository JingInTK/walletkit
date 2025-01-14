//
//  BRBCashParams.c
//  WalletKitCore
//
//  Created by Aaron Voisine on 3/11/19.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "support/BRInt.h"
#include "support/BRSet.h"
#include "bitcoin/BRBitcoinPeer.h"
#include "BRBCashParams.h"

static const char *bchMainNetDNSSeeds[] = {
    "seed-bch.breadwallet.com.",
    "seed.flowee.cash.",
    "seed-bch.bitcoinforks.org.",
    "btccash-seeder.bitcoinunlimited.info.",
    "seed.bchd.cash.",
    "seed.bch.loping.net.",
    "dnsseed.electroncash.de.",
    NULL
};

static const char *bchTestNetDNSSeeds[] = {
    "testnet-seed-bch.breadwallet.com.",
    "testnet-seed-bch.bitcoinforks.org.",
    "testnet-seed.bchd.cash.",
    "seed.tbch.loping.net.",
    NULL
};

#define ASERT_REF_BITS   0x1804dafe
#define ASERT_REF_HEIGHT 661647
#define ASERT_REF_TIME   1605447844
#define ASERT_HALFLIFE   (2*24*60*60)

// aserti3-2d difficulty algorithm: https://upgradespecs.bitcoincashnode.org/2020-11-15-asert/
uint32_t aserti3_2d(uint32_t refBits, int64_t timeDelta, int64_t heightDelta)
{
    // refBits is in "compact" format, where the most significant byte is the size of the value in bytes, next
    // bit is the sign, and the last 23 bits is the value after having been right shifted by (size - 3)*8 bits

    // next_target = old_target * 2^((time_delta - ideal_block_time * (height_delta + 1)) / halflife)
    int64_t exponent = ((timeDelta - 600*(heightDelta + 1))*65536)/ASERT_HALFLIFE;
    int64_t size = refBits >> 24, shifts = exponent >> 16;
    uint64_t factor, target = refBits & 0x007fffff, exp = exponent & 0xffff;

    factor = 65536 + ((195766423245049ULL*exp + 971821376ULL*exp*exp + 5127ULL*exp*exp*exp + (1ULL << 47)) >> 48);
    while (factor && target > ~0ULL/factor) target >>= 8, size++;
    target *= factor;
    shifts -= 16;
    while (size > 3 && shifts < 0) shifts += 8, size--;
    while (shifts >= 8) shifts -= 8, size++;

    if (shifts > 0) {
        while (target > ~0ULL >> shifts) target >>= 8, size++;
        target <<= shifts;
    }
    else target >>= -shifts;
    
    if (target == 0) target = 1;
    while (size < 1 || target > 0x007fffff) target >>= 8, size++; // normalize target for "compact" format
    while (size > 1 && target <= 0x7fff) target <<= 8, size--;
    target |= (uint64_t)size << 24;
    if (target > 0x1d00ffff) target = 0x1d00ffff;
    
    return (uint32_t)target;
}

static int bchMainNetVerifyDifficulty(const BRBitcoinMerkleBlock *block, const BRSet *blockSet)
{
    const BRBitcoinMerkleBlock *prev;
    int64_t timeDelta, heightDelta;
    
    assert(block != NULL);
    assert(blockSet != NULL);
    if (! block) return 0;
    
    if (block->height > ASERT_REF_HEIGHT) { // axion activation height
        prev = BRSetGet(blockSet, &block->prevBlock);
        if (! prev) return 1;
        timeDelta = prev->timestamp - ASERT_REF_TIME;
        heightDelta = prev->height - ASERT_REF_HEIGHT;
        if (aserti3_2d(ASERT_REF_BITS, timeDelta, heightDelta) != block->target) return 0;
    }

    return btcMerkleBlockVerifyProofOfWork(block);
}

static int bchTestNetVerifyDifficulty(const BRBitcoinMerkleBlock *block, const BRSet *blockSet)
{
    return 1; // XXX skip testnet difficulty check for now
}

static BRBitcoinChainParams bchMainNetParamsRecord = {
    bchMainNetDNSSeeds,
    8333,                // standardPort
    0xe8f3e1e3,          // magicNumber
    SERVICES_NODE_BCASH, // services
    bchMainNetVerifyDifficulty,
    NULL,
    0,
    { BITCOIN_PUBKEY_PREFIX, BITCOIN_SCRIPT_PREFIX, BITCOIN_PRIVKEY_PREFIX, NULL },
    BCASH_FORKID,
    BITCOIN_BIP32_DEPTH,
    BITCOIN_BIP32_CHILD
};

static BRBitcoinChainParams bchTestNetParamsRecord = {
    bchTestNetDNSSeeds,
    18333,               // standardPort
    0xf4f3e5f4,          // magicNumber
    SERVICES_NODE_BCASH, // services
    bchTestNetVerifyDifficulty,
    NULL,
    0,
    { BITCOIN_PUBKEY_PREFIX_TEST, BITCOIN_SCRIPT_PREFIX_TEST, BITCOIN_PRIVKEY_PREFIX_TEST, NULL },
    BCASH_FORKID,
    BITCOIN_BIP32_DEPTH_TEST,
    BITCOIN_BIP32_CHILD_TEST
};

// Run once initializer for bsv checkpoints
static void bchChainParamsInit(void) {

    // BCH MainNet
    
    // blockchain checkpoints - these are also used as starting points for partial chain downloads, so they must be at
    // difficulty transition boundaries in order to verify the block difficulty at the immediately following transition
    BRBitcoinCheckPoint bchMainNetCheckpoints[] = {
        {      0, uint256("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"), 1231006505, 0x1d00ffff },
        {  20160, uint256("000000000f1aef56190aee63d33a373e6487132d522ff4cd98ccfc96566d461e"), 1248481816, 0x1d00ffff },
        {  40320, uint256("0000000045861e169b5a961b7034f8de9e98022e7a39100dde3ae3ea240d7245"), 1266191579, 0x1c654657 },
        {  60480, uint256("000000000632e22ce73ed38f46d5b408ff1cff2cc9e10daaf437dfd655153837"), 1276298786, 0x1c0eba64 },
        {  80640, uint256("0000000000307c80b87edf9f6a0697e2f01db67e518c8a4d6065d1d859a3a659"), 1284861847, 0x1b4766ed },
        { 100800, uint256("000000000000e383d43cc471c64a9a4a46794026989ef4ff9611d5acb704e47a"), 1294031411, 0x1b0404cb },
        { 120960, uint256("0000000000002c920cf7e4406b969ae9c807b5c4f271f490ca3de1b0770836fc"), 1304131980, 0x1b0098fa },
        { 141120, uint256("00000000000002d214e1af085eda0a780a8446698ab5c0128b6392e189886114"), 1313451894, 0x1a094a86 },
        { 161280, uint256("00000000000005911fe26209de7ff510a8306475b75ceffd434b68dc31943b99"), 1326047176, 0x1a0d69d7 },
        { 181440, uint256("00000000000000e527fc19df0992d58c12b98ef5a17544696bbba67812ef0e64"), 1337883029, 0x1a0a8b5f },
        { 201600, uint256("00000000000003a5e28bef30ad31f1f9be706e91ae9dda54179a95c9f9cd9ad0"), 1349226660, 0x1a057e08 },
        { 221760, uint256("00000000000000fc85dd77ea5ed6020f9e333589392560b40908d3264bd1f401"), 1361148470, 0x1a04985c },
        { 241920, uint256("00000000000000b79f259ad14635739aaf0cc48875874b6aeecc7308267b50fa"), 1371418654, 0x1a00de15 },
        { 262080, uint256("000000000000000aa77be1c33deac6b8d3b7b0757d02ce72fffddc768235d0e2"), 1381070552, 0x1916b0ca },
        { 282240, uint256("0000000000000000ef9ee7529607286669763763e0c46acfdefd8a2306de5ca8"), 1390570126, 0x1901f52c },
        { 302400, uint256("0000000000000000472132c4daaf358acaf461ff1c3e96577a74e5ebf91bb170"), 1400928750, 0x18692842 },
        { 322560, uint256("000000000000000002df2dd9d4fe0578392e519610e341dd09025469f101cfa1"), 1411680080, 0x181fb893 },
        { 342720, uint256("00000000000000000f9cfece8494800d3dcbf9583232825da640c8703bcd27e7"), 1423496415, 0x1818bb87 },
        { 362880, uint256("000000000000000014898b8e6538392702ffb9450f904c80ebf9d82b519a77d5"), 1435475246, 0x1816418e },
        { 383040, uint256("00000000000000000a974fa1a3f84055ad5ef0b2f96328bc96310ce83da801c9"), 1447236692, 0x1810b289 },
        { 403200, uint256("000000000000000000c4272a5c68b4f55e5af734e88ceab09abf73e9ac3b6d01"), 1458292068, 0x1806a4c3 },
        { 423360, uint256("000000000000000001630546cde8482cc183708f076a5e4d6f51cd24518e8f85"), 1470163842, 0x18057228 },
        { 443520, uint256("00000000000000000345d0c7890b2c81ab5139c6e83400e5bed00d23a1f8d239"), 1481765313, 0x18038b85 },
        { 463680, uint256("000000000000000000431a2f4619afe62357cd16589b638bb638f2992058d88e"), 1493259601, 0x18021b3e },
        { 478559, uint256("000000000000000000651ef99cb9fcbe0dadde1d424bd9f15ff20136191a5eec"), 1501611161, 0x18014735 }, // BCH Hard Fork vv
        { 483840, uint256("00000000000000000098963251fcfc19d0fa2ef05cf22936a182609f8d650346"), 1503802540, 0x1803c5d5 },
        { 504000, uint256("0000000000000000006cdeece5716c9c700f34ad98cb0ed0ad2c5767bbe0bc8c"), 1510516839, 0x18021abd },
        { 524160, uint256("0000000000000000003f40db0a3ed4b4d82b105e212166b2db498d5688bac60f"), 1522711454, 0x18033b64 },
        { 544320, uint256("000000000000000001619f65f7d5ef7a06ee50d2b459cdf727d74b2a7a762268"), 1534794998, 0x18022f7e },
        { 556767, uint256("0000000000000000004626ff6e3b936941d341c5932ece4357eeccac44e6d56c"), 1542304936, 0x18021fdb }, // BSV Hard Fork >>
        { 564480, uint256("0000000000000000020f6ec365b859c7160fbab65652dfc8b52a2664f8ba1e48"), 1546969567, 0x18058b4b },
        { 584640, uint256("000000000000000001d07b1281321b5aa04506e6630dc1e66cf0ad0324d0533c"), 1559087892, 0x18039060 },
        { 604800, uint256("0000000000000000014c579c6ec5dcaf3ed526c5391509c9860b34ed465500a0"), 1571195602, 0x18033efa },
        { 624960, uint256("00000000000000000124d45a12cf1c35cfacc9886216a297fbb5e115c8bd942a"), 1583318442, 0x1801f8db },
        { 645120, uint256("000000000000000001498214c105c8f077879ade00a126a0d4d3aef98cdff2dd"), 1595490461, 0x1802ad71 },
        { 661648, uint256("0000000000000000029e471c41818d24b8b74c911071c4ef0b4a0509f9b5a8ce"), 1605449817, 0x1804e0d0 }, // BCHN Hard Fork vv
        { 665280, uint256("000000000000000001b2c74d7a603fb04c227c83cd935088eb65917070bacb83"), 1607600095, 0x180458b3 }
        // 685440
    };

    // Initialize bsv mainnet checkpoints
    bchMainNetParamsRecord.checkpointsCount = sizeof(bchMainNetCheckpoints) / sizeof(BRBitcoinCheckPoint);
    bchMainNetParamsRecord.checkpoints = calloc (bchMainNetParamsRecord.checkpointsCount, sizeof(BRBitcoinCheckPoint));
    memcpy ((BRBitcoinCheckPoint *) bchMainNetParamsRecord.checkpoints,
            bchMainNetCheckpoints,
            sizeof(bchMainNetCheckpoints));

    // BCH TestNet

    BRBitcoinCheckPoint bchTestNetCheckpoints[] = {
        {       0, uint256("000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"), 1296688602, 0x1d00ffff },
        {  100800, uint256("0000000000a33112f86f3f7b0aa590cb4949b84c2d9c673e9e303257b3be9000"), 1376543922, 0x1c00d907 },
        {  201600, uint256("0000000000376bb71314321c45de3015fe958543afcbada242a3b1b072498e38"), 1393813869, 0x1b602ac0 },
        {  302400, uint256("0000000000001c93ebe0a7c33426e8edb9755505537ef9303a023f80be29d32d"), 1413766239, 0x1a33605e },
        {  403200, uint256("0000000000ef8b05da54711e2106907737741ac0278d59f358303c71d500f3c4"), 1431821666, 0x1c02346c },
        {  504000, uint256("0000000000005d105473c916cd9d16334f017368afea6bcee71629e0fcf2f4f5"), 1436951946, 0x1b00ab86 },
        {  604800, uint256("00000000000008653c7e5c00c703c5a9d53b318837bb1b3586a3d060ce6fff2e"), 1447484641, 0x1a092a20 },
        {  705600, uint256("00000000004ee3bc2e2dd06c31f2d7a9c3e471ec0251924f59f222e5e9c37e12"), 1455728685, 0x1c0ffff0 },
        {  806400, uint256("0000000000000faf114ff29df6dbac969c6b4a3b407cd790d3a12742b50c2398"), 1462006183, 0x1a34e280 },
        {  907200, uint256("0000000000166938e6f172a21fe69fe335e33565539e74bf74eeb00d2022c226"), 1469705562, 0x1c00ffff },
        { 1008000, uint256("000000000000390aca616746a9456a0d64c1bd73661fd60a51b5bf1c92bae5a0"), 1476926743, 0x1a52ccc0 },
        { 1108800, uint256("00000000000288d9a219419d0607fb67cc324d4b6d2945ca81eaa5e739fab81e"), 1490751239, 0x1b09ecf0 },
        { 1209600, uint256("0000000000083e8da119a0dee60f7d8925488f43a9f3591fef0cf8c3b1a86887"), 1517992679, 0x1c01bbf5 },
        { 1310400, uint256("00000000066c529e690ef631773ff540866188a6c6509c73dfc87bc2979322c2"), 1561313963, 0x1c096cde },
        { 1411200, uint256("000000000011a7d78a28f9822f07e8a931dd9a4de1892670665b10f2c81f7c5a"), 1600929524, 0x1b309a2b },
        { 1421484, uint256("00000000289d6afe7e55fe62a7b21b5d0d5481189bb6d9e8dad5e194078e434a"), 1605446610, 0x1d00e466 }  // BCHN Hard Fork vv
                                                                                                                          // 1512000
                                                                                                                          // 1612800
    };

    // Initialize bsv testnet checkpoints
    bchTestNetParamsRecord.checkpointsCount = sizeof(bchTestNetCheckpoints) / sizeof(BRBitcoinCheckPoint);
    bchTestNetParamsRecord.checkpoints = calloc (bchTestNetParamsRecord.checkpointsCount, sizeof(BRBitcoinCheckPoint));
    memcpy ((BRBitcoinCheckPoint *) bchTestNetParamsRecord.checkpoints,
            bchTestNetCheckpoints,
            sizeof(bchTestNetCheckpoints) );
}

// Run once initializer guarantees
static pthread_once_t bchChainParamsInitOnce = PTHREAD_ONCE_INIT;
const BRBitcoinChainParams *bchChainParams(bool mainnet) {
    pthread_once (&bchChainParamsInitOnce, bchChainParamsInit);
    return (mainnet ? &bchMainNetParamsRecord : &bchTestNetParamsRecord);
}
