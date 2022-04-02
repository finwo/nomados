#!/usr/bin/env bash

set -e

target=$1
rootdir=$2

if [ -L "$target" ]; then
	target=$(readlink -f "$target")
	$0 "$target" "$rootdir" || true
elif [ -f "$target" ]; then
	libs=$(ldd "$target" | egrep -o '/[^ ]+')
	for lib in $libs; do
		mkdir -p $(dirname "$rootdir$lib")
		cp -n "$lib" "$rootdir$lib"
		$0 "$lib" "$rootdir" || true
	done
fi
