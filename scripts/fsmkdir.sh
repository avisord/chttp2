#!/bin/sh
# POST /fs/mkdir  body: {"path":"<path>"}
# Usage: fsmkdir.sh <remote-path>
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1

if [ -z "$path" ]; then
  echo "Usage: $0 <remote-path>"
  exit 1
fi

curl $flags -X POST "$server/fs/mkdir" \
  -H "Content-Type: application/json" \
  -d "$(printf '{"path":"%s"}' "$path")"
