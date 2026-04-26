# Copyright (c) 2024-present The BitcoinIII Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

set(CMAKE_AUTOMOC_MOC_OPTIONS "-p${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(test_bitcoinIII-qt
  apptests.cpp
  optiontests.cpp
  rpcnestedtests.cpp
  test_main.cpp
  uritests.cpp
  util.cpp
  ../../init/bitcoinIII-qt.cpp
)

target_link_libraries(test_bitcoinIII-qt
  core_interface
  bitcoinIIIqt
  test_util
  bitcoinIII_node
  Boost::headers
  Qt5::Test
)

import_plugins(test_bitcoinIII-qt)

if(ENABLE_WALLET)
  target_sources(test_bitcoinIII-qt
    PRIVATE
      addressbooktests.cpp
      wallettests.cpp
      ../../wallet/test/wallet_test_fixture.cpp
  )
endif()

add_test(NAME test_bitcoinIII-qt
  COMMAND test_bitcoinIII-qt
)
if(WIN32 AND VCPKG_TARGET_TRIPLET)
  # On Windows, vcpkg configures Qt with `-opengl dynamic`, which makes
  # the "minimal" platform plugin unusable due to internal Qt bugs.
  set_tests_properties(test_bitcoinIII-qt PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
  )
endif()

install_binary_component(test_bitcoinIII-qt)
