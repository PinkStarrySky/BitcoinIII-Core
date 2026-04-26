// Copyright (c) 2022 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOINIII_NODE_DATABASE_ARGS_H
#define BITCOINIII_NODE_DATABASE_ARGS_H

class ArgsManager;
struct DBOptions;

namespace node {
void ReadDatabaseArgs(const ArgsManager& args, DBOptions& options);
} // namespace node

#endif // BITCOINIII_NODE_DATABASE_ARGS_H
