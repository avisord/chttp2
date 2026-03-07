#!/bin/sh
# DELETE /fs/file?path=<path>
# Usage: fsdelfile.sh <remote-path>
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1

if [ -z "$path" ]; then
  echo "Usage: $0 <remote-path>"
  exit 1
fi

curl $flags -X DELETE "$server/fs/file?path=$path"
