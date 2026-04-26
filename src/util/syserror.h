// Copyright (c) 2010-2022 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOINIII_UTIL_SYSERROR_H
#define BITCOINIII_UTIL_SYSERROR_H

#include <string>

/** Return system error string from errno value. Use this instead of
 * std::strerror, which is not thread-safe. For network errors use
 * NetworkErrorString from sock.h instead.
 */
std::string SysErrorString(int err);

#if defined(WIN32)
std::string Win32ErrorString(int err);
#endif

#endif // BITCOINIII_UTIL_SYSERROR_H
