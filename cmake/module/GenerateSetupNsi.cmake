# Copyright (c) 2023-present The BitcoinIII Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_setup_nsi)
  set(abs_top_srcdir ${PROJECT_SOURCE_DIR})
  set(abs_top_builddir ${PROJECT_BINARY_DIR})
  set(CLIENT_URL ${PROJECT_HOMEPAGE_URL})
  set(CLIENT_TARNAME "bitcoinIII")
  set(BITCOINIII_GUI_NAME "bitcoinIII-qt")
  set(BITCOINIII_DAEMON_NAME "bitcoinIIId")
  set(BITCOINIII_CLI_NAME "bitcoinIII-cli")
  set(BITCOINIII_TX_NAME "bitcoinIII-tx")
  set(BITCOINIII_WALLET_TOOL_NAME "bitcoinIII-wallet")
  set(BITCOINIII_TEST_NAME "test_bitcoinIII")
  set(EXEEXT ${CMAKE_EXECUTABLE_SUFFIX})
  configure_file(${PROJECT_SOURCE_DIR}/share/setup.nsi.in ${PROJECT_BINARY_DIR}/bitcoinIII-win64-setup.nsi USE_SOURCE_PERMISSIONS @ONLY)
endfunction()
