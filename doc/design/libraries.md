# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libbitcoinIII_cli*         | RPC client functionality used by *bitcoinIII-cli* executable |
| *libbitcoinIII_common*      | Home for common functionality shared by different executables and libraries. Similar to *libbitcoinIII_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libbitcoinIII_consensus*   | Consensus functionality used by *libbitcoinIII_node* and *libbitcoinIII_wallet*. |
| *libbitcoinIII_crypto*      | Hardware-optimized functions for data encryption, hashing, message authentication, and key derivation. |
| *libbitcoinIII_kernel*      | Consensus engine and support library used for validation by *libbitcoinIII_node*. |
| *libbitcoinIIIqt*           | GUI functionality used by *bitcoinIII-qt* and *bitcoinIII-gui* executables. |
| *libbitcoinIII_ipc*         | IPC functionality used by *bitcoinIII-node*, *bitcoinIII-wallet*, *bitcoinIII-gui* executables to communicate when [`-DWITH_MULTIPROCESS=ON`](multiprocess.md) is used. |
| *libbitcoinIII_node*        | P2P and RPC server functionality used by *bitcoinIIId* and *bitcoinIII-qt* executables. |
| *libbitcoinIII_util*        | Home for common functionality shared by different executables and libraries. Similar to *libbitcoinIII_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libbitcoinIII_wallet*      | Wallet functionality used by *bitcoinIIId* and *bitcoinIII-wallet* executables. |
| *libbitcoinIII_wallet_tool* | Lower-level wallet functionality used by *bitcoinIII-wallet* executable. |
| *libbitcoinIII_zmq*         | [ZeroMQ](../zmq.md) functionality used by *bitcoinIIId* and *bitcoinIII-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. An exception is *libbitcoinIII_kernel*, which, at some future point, will have a documented external interface.

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`add_library(bitcoinIII_* ...)`](../../src/CMakeLists.txt) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libbitcoinIII_node* code lives in `src/node/` in the `node::` namespace
  - *libbitcoinIII_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libbitcoinIII_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libbitcoinIII_util* code lives in `src/util/` in the `util::` namespace
  - *libbitcoinIII_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

bitcoinIII-cli[bitcoinIII-cli]-->libbitcoinIII_cli;

bitcoinIIId[bitcoinIIId]-->libbitcoinIII_node;
bitcoinIIId[bitcoinIIId]-->libbitcoinIII_wallet;

bitcoinIII-qt[bitcoinIII-qt]-->libbitcoinIII_node;
bitcoinIII-qt[bitcoinIII-qt]-->libbitcoinIIIqt;
bitcoinIII-qt[bitcoinIII-qt]-->libbitcoinIII_wallet;

bitcoinIII-wallet[bitcoinIII-wallet]-->libbitcoinIII_wallet;
bitcoinIII-wallet[bitcoinIII-wallet]-->libbitcoinIII_wallet_tool;

libbitcoinIII_cli-->libbitcoinIII_util;
libbitcoinIII_cli-->libbitcoinIII_common;

libbitcoinIII_consensus-->libbitcoinIII_crypto;

libbitcoinIII_common-->libbitcoinIII_consensus;
libbitcoinIII_common-->libbitcoinIII_crypto;
libbitcoinIII_common-->libbitcoinIII_util;

libbitcoinIII_kernel-->libbitcoinIII_consensus;
libbitcoinIII_kernel-->libbitcoinIII_crypto;
libbitcoinIII_kernel-->libbitcoinIII_util;

libbitcoinIII_node-->libbitcoinIII_consensus;
libbitcoinIII_node-->libbitcoinIII_crypto;
libbitcoinIII_node-->libbitcoinIII_kernel;
libbitcoinIII_node-->libbitcoinIII_common;
libbitcoinIII_node-->libbitcoinIII_util;

libbitcoinIIIqt-->libbitcoinIII_common;
libbitcoinIIIqt-->libbitcoinIII_util;

libbitcoinIII_util-->libbitcoinIII_crypto;

libbitcoinIII_wallet-->libbitcoinIII_common;
libbitcoinIII_wallet-->libbitcoinIII_crypto;
libbitcoinIII_wallet-->libbitcoinIII_util;

libbitcoinIII_wallet_tool-->libbitcoinIII_wallet;
libbitcoinIII_wallet_tool-->libbitcoinIII_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class bitcoinIII-qt,bitcoinIIId,bitcoinIII-cli,bitcoinIII-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Crypto* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus, crypto, and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libbitcoinIII_wallet* and *libbitcoinIII_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libbitcoinIII_crypto* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libbitcoinIII_consensus* should only depend on *libbitcoinIII_crypto*, and all other libraries besides *libbitcoinIII_crypto* should be allowed to depend on it.

- *libbitcoinIII_util* should be a standalone dependency that any library can depend on, and it should not depend on other libraries except *libbitcoinIII_crypto*. It provides basic utilities that fill in gaps in the C++ standard library and provide lightweight abstractions over platform-specific features. Since the util library is distributed with the kernel and is usable by kernel applications, it shouldn't contain functions that external code shouldn't call, like higher level code targeted at the node or wallet. (*libbitcoinIII_common* is a better place for higher level code, or code that is meant to be used by internal applications only.)

- *libbitcoinIII_common* is a home for miscellaneous shared code used by different BitcoinIII Core applications. It should not depend on anything other than *libbitcoinIII_util*, *libbitcoinIII_consensus*, and *libbitcoinIII_crypto*.

- *libbitcoinIII_kernel* should only depend on *libbitcoinIII_util*, *libbitcoinIII_consensus*, and *libbitcoinIII_crypto*.

- The only thing that should depend on *libbitcoinIII_kernel* internally should be *libbitcoinIII_node*. GUI and wallet libraries *libbitcoinIIIqt* and *libbitcoinIII_wallet* in particular should not depend on *libbitcoinIII_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be able to get it from *libbitcoinIII_consensus*, *libbitcoinIII_common*, *libbitcoinIII_crypto*, and *libbitcoinIII_util*, instead of *libbitcoinIII_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libbitcoinIIIqt*, *libbitcoinIII_node*, *libbitcoinIII_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](../../src/interfaces/) abstract interfaces.

## Work in progress

- Validation code is moving from *libbitcoinIII_node* to *libbitcoinIII_kernel* as part of [The libbitcoinIIIkernel Project #27587](https://github.com/bitcoinIII/bitcoinIII/issues/27587)
