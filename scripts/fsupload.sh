#!/bin/sh
# POST /fs/upload?path=<dir>  <local-file>
# Usage: fsupload.sh <local-file> [remote-dir]
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

file=$1
dir=${2:-.}

if [ -z "$file" ]; then
  echo "Usage: $0 <local-file> [remote-dir]"
  exit 1
fi

curl $flags -X POST "$server/fs/upload?path=$dir" \
  -F "file=@$file"
