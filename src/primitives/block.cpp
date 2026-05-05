// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>

uint256 CBlockHeader::GetHash(int _height, const Consensus::Params& _params) const
{
    if (_height >= _params.SHA3Height)
        return (HashWriterSHA3{} << *this).GetHash();
    return (HashWriter{} << *this).GetHash();
}

uint256 CBlockHeader::GetSHA256dHash() const
{
    return (HashWriter{} << *this).GetHash();
}

uint256 CBlockHeader::GetSHA3_256dHash() const
{
    return (HashWriterSHA3{} << *this).GetHash();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(sha2hash=%s, sha3hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetSHA256dHash().ToString(), // CHANGE
        GetSHA3_256dHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx)
        s << "  " << tx->ToString() << "\n";
    return s.str();
}
