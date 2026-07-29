#!/bin/sh
set -e

version=$1
if [ -z "$version" ]; then
	printf "usage: %s <version>\n" "$0"
	exit 1
fi

exe=bitbit-$version-linux-x86-64
make bitbit-pgo CC=clang ARCH=x86-64-v2 TT=256 STATIC=yes EXE=$exe

exe=bitbit-$version-linux-x86-64-avx2
make bitbit-pgo CC=clang ARCH=x86-64-v3 TT=256 STATIC=yes EXE=$exe SIMD=avx2

exe=bitbit-$version-windows-x86-64.exe
make bitbit-pgo CC=clang ARCH=x86-64-v2 TT=256 STATIC=yes EXE=$exe TARGET=x86_64-w64-mingw32

exe=bitbit-$version-windows-x86-64-avx2.exe
make bitbit-pgo CC=clang ARCH=x86-64-v3 TT=256 STATIC=yes EXE=$exe SIMD=avx2 TARGET=x86_64-w64-mingw32
