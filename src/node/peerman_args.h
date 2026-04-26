#ifndef BITCOINIII_NODE_PEERMAN_ARGS_H
#define BITCOINIII_NODE_PEERMAN_ARGS_H

#include <net_processing.h>

class ArgsManager;

namespace node {
void ApplyArgsManOptions(const ArgsManager& argsman, PeerManager::Options& options);
} // namespace node

#endif // BITCOINIII_NODE_PEERMAN_ARGS_H
