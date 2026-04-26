// Copyright (c) 2011-2020 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOINIII_MAPPORT_H
#define BITCOINIII_MAPPORT_H

static constexpr bool DEFAULT_NATPMP = false;

void StartMapPort(bool enable);
void InterruptMapPort();
void StopMapPort();

#endif // BITCOINIII_MAPPORT_H
