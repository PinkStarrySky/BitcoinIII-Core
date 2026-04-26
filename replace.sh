#!/bin/bash

if [ $# -lt 1 ]; then
  exit 1
fi

for arg in "$@"; do
  if ! [ -f "$arg" ]; then
    echo "$arg is not a file."
    continue;
  fi
  echo "Replacing: $arg"
  sed -i 's/BitcoinII/BitcoinIII/g' "$arg"
  sed -i 's/BitcoinIIII/BitcoinIII/g' "$arg"
  sed -i 's/Bitcoin-II/Bitcoin-III/g' "$arg"
  sed -i 's/Bitcoin-IIII/Bitcoin-III/g' "$arg"

  sed -i 's/bitcoinII/bitcoinIII/g' "$arg"
  sed -i 's/bitcoinIIII/bitcoinIII/g' "$arg"
  sed -i 's/bitcoin-II/bitcoin-III/g' "$arg"
  sed -i 's/bitcoin-IIII/bitcoin-III/g' "$arg"

  sed -i 's/Bitcoinii/Bitcoiniii/g' "$arg"
  sed -i 's/Bitcoiniiii/Bitcoiniii/g' "$arg"
  sed -i 's/Bitcoin-ii/Bitcoin-iii/g' "$arg"
  sed -i 's/Bitcoin-iiii/Bitcoin-iii/g' "$arg"

  sed -i 's/bitcoinii/bitcoiniii/g' "$arg"
  sed -i 's/bitcoiniiii/bitcoiniii/g' "$arg"
  sed -i 's/bitcoin-ii/bitcoin-iii/g' "$arg"
  sed -i 's/bitcoin-iiii/bitcoin-iii/g' "$arg"

  sed -i 's/BITCOINII/BITCOINIII/g' "$arg"
  sed -i 's/BITCOINIIII/BITCOINIII/g' "$arg"
  sed -i 's/BITCOIN-II/BITCOIN-III/g' "$arg"
  sed -i 's/BITCOIN-IIII/BITCOIN-III/g' "$arg"

  sed -i 's/sat2/sat3/g' "$arg"
  sed -i 's/SAT2/SAT3/g' "$arg"

  sed -i 's/satooshi/satoooshi/g' "$arg"
  sed -i 's/Satooshi/Satoooshi/g' "$arg"
  sed -i 's/SATOOSHI/SATOOOSHI/g' "$arg"

  sed -i 's/BC2/BC3/g' "$arg" # this breaks stuff in THREE PLACES! Will manually fix after.

  src="$arg"
  dst="${arg//bitcoinII/bitcoinIII}"
  dst="${dst//bitcoinIIII/bitcoinIII}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//bitcoin-II/bitcoin-III}"
  dst="${dst//bitcoin-IIII/bitcoin-III}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//Bitcoin-II/Bitcoin-III}"
  dst="${dst//Bitcoin-IIII/Bitcoin-III}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//BITCOINII/BITCOINIII}"
  dst="${dst//BITCOINIIII/BITCOINIII}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//BITCOIN-II/BITCOIN-III}"
  dst="${dst//BITCOIN-IIII/BITCOIN-III}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//bitcoinii/bitcoiniii}"
  dst="${dst//bitcoiniiii/bitcoiniii}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//bitcoin-ii/bitcoin-iii}"
  dst="${dst//bitcoin-iiii/bitcoin-iii}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//Bitcoin-ii/Bitcoin-iii}"
  dst="${dst//Bitcoin-iiii/Bitcoin-iii}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//BITCOINii/BITCOINiii}"
  dst="${dst//BITCOINiiii/BITCOINiii}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi

  src="$arg"
  dst="${arg//BITCOIN-ii/BITCOIN-iii}"
  dst="${dst//BITCOIN-iiii/BITCOIN-iii}"
  if [ "$src" != "$dst" ]; then
    mv "$src" "$dst"
    arg="$dst"
  fi
done
