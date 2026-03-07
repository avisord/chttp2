#!/bin/sh
# POST /fs/copy  body: {"from":"<src>","to":"<dst>"}
# Usage: fscopy.sh <from-path> <to-path>
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

from=$1
to=$2

if [ -z "$from" ] || [ -z "$to" ]; then
  echo "Usage: $0 <from-path> <to-path>"
  exit 1
fi

curl $flags -X POST "$server/fs/copy" \
  -H "Content-Type: application/json" \
  -d "$(printf '{"from":"%s","to":"%s"}' "$from" "$to")"
