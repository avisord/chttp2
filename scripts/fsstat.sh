#!/bin/sh
# GET /fs/stat?path=<path>
# Usage: fsstat.sh <remote-path>
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1

if [ -z "$path" ]; then
  echo "Usage: $0 <remote-path>"
  exit 1
fi

curl $flags "$server/fs/stat?path=$path"
