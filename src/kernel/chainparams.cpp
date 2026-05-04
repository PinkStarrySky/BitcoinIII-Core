// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>
#include <arith_uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cinttypes>
#include <thread>
#include <fstream>
#include <type_traits>

using namespace util::hex_literals;

// Workaround MSVC bug triggering C7595 when calling consteval constructors in
// initializer lists.
// A fix may be on the way:
// https://developercommunity.visualstudio.com/t/consteval-conversion-function-fails/1579014
#if defined(_MSC_VER)
auto consteval_ctor(auto&& input) { return input; }
#else
#define consteval_ctor(input) (input)
#endif

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "IMF 15/Apr/2026 Fiscal Policy Under Pressure: High Debt, Rising Risks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

static void MineGenesisBlockThread(CBlock *global_genesis,
                                   CBlock  genesis,
                                   std::mutex *mutex,
                                   std::atomic<bool> *bye,
                                   uint32_t nthreads,
                                   uint32_t soffset,
                                   arith_uint256 *target,
                                   arith_uint256 *global_best,
                                   uint64_t prog = 0) {
    arith_uint256 local_best = UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"});
    arith_uint256 hash;
    genesis.nNonce += soffset;
    uint32_t prev;
    while (!bye->load() && (hash = UintToArith256(genesis.GetHash())) > *target) {
        if (hash < local_best) {
            local_best = hash;
            if (local_best < *global_best) {
                mutex->lock();
                std::fprintf(stdout,
                             "New best: %s\nnNonce: %u\nnTime: %u\n--------\n",
                             genesis.GetHash().ToString().c_str(),
                             genesis.nNonce,
                             genesis.nTime);
                *global_best = local_best;
                mutex->unlock();
            }
        }
        prev = genesis.nNonce;
        genesis.nNonce += nthreads;
        if (genesis.nNonce < prev)
            ++genesis.nTime;
    }
    mutex->lock();
    if (bye->load()) {
        mutex->unlock();
        return;
    }
    bye->store(true);
    *global_best = hash;
    *global_genesis = genesis;
    mutex->unlock();
}

static void MineGenesisBlock(CBlock &genesis,
                             const char *network,
                             uint32_t override_nBits = 0x1c0fffff,
                             uint64_t prog = 100'000'000) {
    bool negative = false;
    bool overflow = false;
    arith_uint256 target;
    target.SetCompact(override_nBits ? override_nBits : genesis.nBits, &negative, &overflow);
    if (negative || overflow || target == 0) {
        std::fprintf(stderr, "Error: bad target!\n");
        std::abort();
    }
    arith_uint256 best = UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"});
    arith_uint256 hash;
    std::mutex mutex;
    int32_t hthreads = std::thread::hardware_concurrency();
    uint32_t nthreads = hthreads <= 0 ? 1 : hthreads;
    std::atomic<bool> bye = false;
    uint32_t counter = 0;
    std::vector<std::thread> threads;
    while (counter < nthreads) {
        threads.emplace_back(&MineGenesisBlockThread,
                             &genesis,
                             genesis,
                             &mutex,
                             &bye,
                             nthreads,
                             counter++,
                             &target,
                             &best,
                             prog);
    }
    for (std::thread &t : threads)
        t.join();
    std::fprintf(stdout,
                 "FOUND %s GENESIS BLOCK!\nHash: %s\nnNonce: %u\nnTime: %u\nMerkleRoot: %s\n",
                 network,
                 genesis.GetHash().ToString().c_str(),
                 genesis.nNonce,
                 genesis.nTime,
                 genesis.hashMerkleRoot.ToString().c_str());
    std::string opath = network;
    opath += "-GENESIS.json";
    std::ofstream out{opath, std::ios_base::out | std::ios_base::trunc};
    if (out.good())
        out << "{\n  \"network\": \"" << network << "\",\n  \"hash\": \"" << genesis.GetHash().ToString() << "\",\n  \"nNonce\": " << genesis.nNonce << ",\n \"nTime\": " << genesis.nTime << ",\n  \"merkle_root\": \"" << genesis.hashMerkleRoot.ToString() << "\"\n}\n";
    out.close();
    std::abort();
}

/* static void MineGenesisBlock(CBlock &genesis, const char *network, uint64_t prog = 0, uint64_t starting_its = 0) {
    bool negative = false;
    bool overflow = false;
    arith_uint256 target;
    target.SetCompact(genesis.nBits, &negative, &overflow);
    if (negative || overflow || target == 0) {
        std::fprintf(stderr, "Error: bad target!\n");
        std::abort();
    }
    arith_uint256 best = UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"});
    arith_uint256 hash;
    uint64_t its = 0; // number of iterations from the starting Genesis block parameters required to find target hash
    if (starting_its) {
        its = starting_its;
        uint64_t bigg = 0x0000000100000000ull;
        genesis.nTime  += (its / bigg);
        genesis.nNonce += (its % bigg); // I will not add 1, so the very first hash will be a repeat.
    }
    while ((hash = UintToArith256(genesis.GetHash())) > target) {
        if (hash < best) {
            std::fprintf(stdout,
                         "New best: %s\nnNonce: %u\nnTime: %u\n----------------\n",
                         genesis.GetHash().ToString().c_str(),
                         genesis.nNonce,
                         genesis.nTime);
            best = hash;
        }
        ++genesis.nNonce;
        if (genesis.nNonce == 0) {
            ++genesis.nTime;
            std::fprintf(stdout, "Had to wrap nonce - incremented time.\n");
        }
        if (prog && its % prog == 0)
            std::fprintf(stdout, "Iterations: %" PRIu64 "\n", its);
        ++its;
    }
    std::fprintf(stdout,
                 "FOUND %s GENESIS BLOCK!\nHash: %s\nnNonce: %u\nnTime: %u\nMerkleRoot: %s\nIterations: %" PRIu64 "\n",
                 network,
                 genesis.GetHash().ToString().c_str(),
                 genesis.nNonce,
                 genesis.nTime,
                 genesis.hashMerkleRoot.ToString().c_str(),
                 its);
    std::string opath = network;
    opath += "-GENESIS.json";
    std::ofstream out{opath, std::ios_base::out | std::ios_base::trunc};
    if (out.good())
        out << "{\n  \"network\": \"" << network << "\",\n  \"hash\": \"" << genesis.GetHash().ToString() << "\",\n  \"nNonce\": " << genesis.nNonce << ",\n \"nTime\": " << genesis.nTime << ",\n  \"merkle_root\": \"" << genesis.hashMerkleRoot.ToString() << "\"\n}\n";
    out.close();
    std::abort();
} */

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        /* consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256{"000000000c226a41e70717f6d4fbdcb6bfb4fdc40831ccc87fa9cfdd2c57bff6"}, SCRIPT_VERIFY_NONE);
        consensus.script_flag_exceptions.emplace( // Taproot exception
            uint256{"000000000c226a41e70717f6d4fbdcb6bfb4fdc40831ccc87fa9cfdd2c57bff6"}, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS); */
        consensus.BIP34Height = 250;
        consensus.BIP34Hash = uint256{"0000000000009aaeefa1cf580a717eb5b87a2f03bb93692f639cf72eb1e888ee"};
        consensus.BIP65Height = 260; //
        consensus.BIP66Height = 270; //
        consensus.CSVHeight = 280; //
        consensus.SegwitHeight = 290; //
        consensus.MinBIP9WarningHeight = 2306; // segwit activation height + miner confirmation window
        consensus.SHA3Height = 28'224; // SHA3-256d PoW Fork Height (placeholder)
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1777300000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout =   1777300000 + 31536000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period    = 2016;

        consensus.nMinimumChainWork = uint256{"00000000000000000000000000000000000000000000000146c1442144214180"}; // Chainwork at block 22810
        consensus.defaultAssumeValid = uint256{"00000000000003a8c623c46ffb4c99bef3c9b662b6de81364d41be0c56f988cf"}; // Block 22810

        pchMessageStart[0] = 0x8a;
        pchMessageStart[1] = 0x34;
        pchMessageStart[2] = 0x58;
        pchMessageStart[3] = 0x76;
        nDefaultPort = 38338;
        nPruneAfterHeight = 200000;
        m_assumed_blockchain_size = 0; // 10;
        m_assumed_chain_state_size = 0; // 10;

        // genesis = CreateGenesisBlock(1777245001, 360582665, 0x1d00ffff, 1, 50 * COIN);
        genesis = CreateGenesisBlock(1777245555, 2442659435, 0x1d00ffff, 1, 50 * COIN);
        // MineGenesisBlock(genesis, "MAINNET", 0x1c00ffff);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000000000c226a41e70717f6d4fbdcb6bfb4fdc40831ccc87fa9cfdd2c57bff6"});
        assert(genesis.hashMerkleRoot == uint256{"8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1"});

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        // vSeeds.emplace_back("dnsseed.bitcoin-iii.org."); CHANGE - add once it exists
        // vSeeds.emplace_back("bitcoinIII.ddns.net."); // CHANGE - add once it exists
        vSeeds.clear();
        // vFixedSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "bc";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {     0, uint256{"000000000c226a41e70717f6d4fbdcb6bfb4fdc40831ccc87fa9cfdd2c57bff6"}}, // Genesis block
                {   250, uint256{"0000000000009aaeefa1cf580a717eb5b87a2f03bb93692f639cf72eb1e888ee"}}, // BIP34 activation height
                {   256, uint256{"000000001346683ec691296457ef5e64aa7f70fa041a56731f2cbdb651fee746"}},
                {   512, uint256{"0000000032ec0a31ac6fa599bebc9202ab1c196edd4a919c33f8324df3dee46a"}},
                {  1024, uint256{"00000000b9e8f3af6d7f9cd48e18ee66b91fe4d281f968f1efc475f09a6b99a3"}},
                {  2016, uint256{"000000000001e14d2befeee1446b8c908f62a4d147088e82cc1001f8d585ea2f"}}, // Epoch 1 start
                {  2048, uint256{"0000000026633fe101b56ec7a3c3423e3e29ea37184cd78c1ece16005e26073c"}},
                {  4032, uint256{"00000000085eb69bcd6ecb2d9f0063efb59fc19ab82d3afa2733b6fdbff1e7eb"}}, // Epoch 2 start
                {  4096, uint256{"0000000000000491b329c02a5d3b870f781a598f6dc73147cfaee907b27d2caf"}},
                {  6048, uint256{"00000000018c3a9ca9e8b750c1c052141abb7d5f0904de45a9678ff21d14c2f6"}}, // Taproot activation height
                {  8192, uint256{"0000000000054fc3668111eb9da553ea90cc1588e0353b507d0ab21e54b427a6"}},
                { 16384, uint256{"00000000000051c8cab3a4e06d637de00eef696596f462ec51b25e7b0ceacaf9"}},
                { 22810, uint256{"00000000000003a8c623c46ffb4c99bef3c9b662b6de81364d41be0c56f988cf"}}
                
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000000001984b0fe236bec1ec49a14a01d8d340c31cae864d96a3b9b
            /* .nTime    = 1764034154,
            .tx_count = 82281,
            .dTxRate  = 0.004174125833742384, */
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        /* consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256{"0000000022d1be6a55d804e774153f263f29523ff058cb2a00376362da78a8f6"}, SCRIPT_VERIFY_NONE); */
        consensus.BIP34Height = 250;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 260; //
        consensus.BIP66Height = 270; //
        consensus.CSVHeight = 280; //
        consensus.SegwitHeight = 290; //
        consensus.MinBIP9WarningHeight = 2306; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1777300000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout   = 1777300000 + 31536000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period    = 2016;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000000000000000000"};
        consensus.defaultAssumeValid = uint256{"0000000022d1be6a55d804e774153f263f29523ff058cb2a00376362da78a8f6"};

        pchMessageStart[0] = 0xb4;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0x42;
        pchMessageStart[3] = 0xc8;
        nDefaultPort = 28338;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 200;
        m_assumed_chain_state_size = 19;

        /*
         * 0000000022d1be6a55d804e774153f263f29523ff058cb2a00376362da78a8f6
         * 102859864
         * 1777250000
         * 8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1
         */
        genesis = CreateGenesisBlock(1777250000, 102859864, 0x1d00ffff, 1, 50 * COIN); // same Merkle root, different block hash
        // MineGenesisBlock(genesis, "TESTNET");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"0000000022d1be6a55d804e774153f263f29523ff058cb2a00376362da78a8f6"});
        assert(genesis.hashMerkleRoot == uint256{"8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1"});
        
        
        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        // vSeeds.emplace_back("testnet-seed.bitcoinIII.ddns.net.");
        

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0, uint256{"0000000022d1be6a55d804e774153f263f29523ff058cb2a00376362da78a8f6"}}
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000003fc7967410ba2d0a8a8d50daedc318d43e8baf1a9782c236a57
            /* .nTime    = 1741042082,
            .tx_count = 475477615,
            .dTxRate  = 17.15933950357594, */
        };
    }
};

/**
 * Testnet (v4): public test network which is reset from time to time.
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params() {
        m_chain_type = ChainType::TESTNET4;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        /* consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256{"000000005541e67eb6f1851976a2269faf7aeb5195df05e4cd0f9a7649852e7d"}, SCRIPT_VERIFY_NONE);
        consensus.script_flag_exceptions.emplace( // Taproot exception
            uint256{"000000005541e67eb6f1851976a2269faf7aeb5195df05e4cd0f9a7649852e7d"}, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS); */
        consensus.BIP34Height = 250;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 260; //
        consensus.BIP66Height = 270; //
        consensus.CSVHeight = 280; //
        consensus.SegwitHeight = 290; //
        consensus.MinBIP9WarningHeight = 2306; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout   = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period    = 2016;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000000000000000000"};
        consensus.defaultAssumeValid = uint256{"0000000011290b093f8422d064715932d33a17c7ff35a9f965feb3ac73b8dbdc"}; // 0

        pchMessageStart[0] = 0x2f;
        pchMessageStart[1] = 0xac;
        pchMessageStart[2] = 0xc5;
        pchMessageStart[3] = 0xa7;
        nDefaultPort = 58338;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0; //1;
        m_assumed_chain_state_size = 0; //1;

        const char* testnet4_genesis_msg = "IMF 15/Apr/2026 Fiscal Policy Under Pressure: High Debt, Rising Risks";
        const CScript testnet4_genesis_script = CScript() << "0400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"_hex << OP_CHECKSIG;
        genesis = CreateGenesisBlock(testnet4_genesis_msg,
                testnet4_genesis_script,
                1777257000,
                1609338968,
                0x1d00ffff,
                1,
                50 * COIN);
        // MineGenesisBlock(genesis, "TESTNET4");
        /*
         * 0000000011290b093f8422d064715932d33a17c7ff35a9f965feb3ac73b8dbdc
         * 1609338968
         * 1777257000
         * 6f24d7660ff79dad7cbab144b3da40e063ea34c50ce959b0efcc9bc8d2bbef50
         */
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"0000000011290b093f8422d064715932d33a17c7ff35a9f965feb3ac73b8dbdc"});
        assert(genesis.hashMerkleRoot == uint256{"6f24d7660ff79dad7cbab144b3da40e063ea34c50ce959b0efcc9bc8d2bbef50"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        // vSeeds.emplace_back("seed.testnet4.bitcoinIII.ddns.net.");
        

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_testnet4), std::end(chainparams_seed_testnet4));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0, uint256{"0000000011290b093f8422d064715932d33a17c7ff35a9f965feb3ac73b8dbdc"}}
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 0000000000003ed4f08dbdf6f7d6b271a6bcffce25675cb40aa9fa43179a89f3
            /* .nTime    = 1741070246,
            .tx_count = 7653966,
            .dTxRate  = 1.239174414591965, */
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vFixedSeeds.clear();
        vSeeds.clear();

        if (!options.challenge) {
            bin = "512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae"_hex_v_u8;
            vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_signet), std::end(chainparams_seed_signet));
            // vSeeds.emplace_back("seed.signet.bitcoinIII.ddns.net.");
            

            consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000000000000000000"};
            consensus.defaultAssumeValid = uint256{"0000000002945238d1ab4bcc22adbf6cdd9f3f72c907d298d55d70a885657854"};
            m_assumed_blockchain_size = 0; //9;
            m_assumed_chain_state_size = 0; //1;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000895a110f46e59eb82bbc5bfb67fa314656009c295509c21b4999f5180a
                /* .nTime    = 1741019645,
                .tx_count = 16540736,
                .dTxRate  = 1.064918879911595, */
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000377ae000000000000000000000000000000000000000000000000000000"};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout   = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period    = 2016;

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 48338;
        nPruneAfterHeight = 1000;

        /*
         * 0000000002945238d1ab4bcc22adbf6cdd9f3f72c907d298d55d70a885657854
         * 753062192
         * 1777260000
         * 8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1
         */

        genesis = CreateGenesisBlock(1777260000, 753062192, 0x1d00ffff, 1, 50 * COIN);
        // MineGenesisBlock(genesis, "SIGNET");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"0000000002945238d1ab4bcc22adbf6cdd9f3f72c907d298d55d70a885657854"});
        assert(genesis.hashMerkleRoot == uint256{"8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1"});

        m_assumeutxo_data = {};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout   = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period    = 144;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xf1;
        pchMessageStart[1] = 0xb9;
        pchMessageStart[2] = 0xeb;
        pchMessageStart[3] = 0xd0;
        nDefaultPort = 28448;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        /*
         * 000000004978cc1c47ca1c84eb26c3797691e8a3a5699736ddb57b04e8f947e5
         * 2363255594
         * 1777261000
         * 8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1
         */

        genesis = CreateGenesisBlock(1777261000,
                                     2363255594,
                                     0x1d00ffff,
                                     1,
                                     50 * COIN);
        // MineGenesisBlock(genesis, "REGTEST");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000000004978cc1c47ca1c84eb26c3797691e8a3a5699736ddb57b04e8f947e5"});
        assert(genesis.hashMerkleRoot == uint256{"8e1df52fddd25c460304ff8ea7bcb570850bf0b0c027eecf8ebf8ab17d3e93b1"});

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256{"000000004978cc1c47ca1c84eb26c3797691e8a3a5699736ddb57b04e8f947e5"}},
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet4()
{
    return std::make_unique<const CTestNet4Params>();
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();
    const auto signet_msg = CChainParams::SigNet({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
