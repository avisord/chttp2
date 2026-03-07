#!/bin/sh
# GET /fs/download?path=<path>
# Usage: fsdownload.sh <remote-path> [output-file]
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1
out=${2:-}

if [ -z "$path" ]; then
  echo "Usage: $0 <remote-path> [output-file]"
  exit 1
fi

if [ -n "$out" ]; then
  curl $flags -o "$out" "$server/fs/download?path=$path"
else
  curl $flags "$server/fs/download?path=$path"
fi
