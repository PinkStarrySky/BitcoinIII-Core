// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOINIII_POLICY_FEERATE_H
#define BITCOINIII_POLICY_FEERATE_H

#include <consensus/amount.h>
#include <serialize.h>


#include <cstdint>
#include <string>
#include <type_traits>

const std::string CURRENCY_UNIT = "BC3"; // One formatted unit
const std::string CURRENCY_ATOM = "sat3"; // One indivisible minimum value unit

/* Used to determine type of fee estimation requested */
enum class FeeEstimateMode {
    UNSET,        //!< Use default settings based on other criteria
    ECONOMICAL,   //!< Force estimateSmartFee to use non-conservative estimates
    CONSERVATIVE, //!< Force estimateSmartFee to use conservative estimates
    BC3_KVB,      //!< Use BC3/kvB fee rate unit
    SAT3_VB,       //!< Use sat3/vB fee rate unit
};

/**
 * Fee rate in satoooshis per kilovirtualbyte: CAmount / kvB
 */
class CFeeRate
{
private:
    /** Fee rate in sat3/kvB (satoooshis per 1000 virtualbytes) */
    CAmount nSatoooshisPerK;

public:
    /** Fee rate of 0 satoooshis per kvB */
    CFeeRate() : nSatoooshisPerK(0) { }
    template<std::integral I> // Disallow silent float -> int conversion
    explicit CFeeRate(const I _nSatoooshisPerK): nSatoooshisPerK(_nSatoooshisPerK) {
    }

    /**
     * Construct a fee rate from a fee in satoooshis and a vsize in vB.
     *
     * param@[in]   nFeePaid    The fee paid by a transaction, in satoooshis
     * param@[in]   num_bytes   The vsize of a transaction, in vbytes
     */
    CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes);

    /**
     * Return the fee in satoooshis for the given vsize in vbytes.
     * If the calculated fee would have fractional satoooshis, then the
     * returned fee will always be rounded up to the nearest satoooshi.
     */
    CAmount GetFee(uint32_t num_bytes) const;

    /**
     * Return the fee in satoooshis for a vsize of 1000 vbytes
     */
    CAmount GetFeePerK() const { return nSatoooshisPerK; }
    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK < b.nSatoooshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK > b.nSatoooshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK == b.nSatoooshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK <= b.nSatoooshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK >= b.nSatoooshisPerK; }
    friend bool operator!=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoooshisPerK != b.nSatoooshisPerK; }
    CFeeRate& operator+=(const CFeeRate& a) { nSatoooshisPerK += a.nSatoooshisPerK; return *this; }
    std::string ToString(const FeeEstimateMode& fee_estimate_mode = FeeEstimateMode::BC3_KVB) const;
    friend CFeeRate operator*(const CFeeRate& f, int a) { return CFeeRate(a * f.nSatoooshisPerK); }
    friend CFeeRate operator*(int a, const CFeeRate& f) { return CFeeRate(a * f.nSatoooshisPerK); }

    SERIALIZE_METHODS(CFeeRate, obj) { READWRITE(obj.nSatoooshisPerK); }
};

#endif // BITCOINIII_POLICY_FEERATE_H
