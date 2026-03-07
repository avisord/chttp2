#!/bin/sh
# POST /fs/rename  body: {"path":"<path>","name":"<newname>"}
# Usage: fsrename.sh <remote-path> <new-name>
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1
name=$2

if [ -z "$path" ] || [ -z "$name" ]; then
  echo "Usage: $0 <remote-path> <new-name>"
  exit 1
fi

curl $flags -X POST "$server/fs/rename" \
  -H "Content-Type: application/json" \
  -d "$(printf '{"path":"%s","name":"%s"}' "$path" "$name")"
