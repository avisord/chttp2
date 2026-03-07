#!/bin/sh
# PUT /fs/content?path=<path>  body: raw text or file content
# Usage: fswrite.sh <remote-path> <text-or-@file>
#   fswrite.sh notes.txt "hello world"
#   fswrite.sh notes.txt @localfile.txt
server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"

path=$1
body=$2

if [ -z "$path" ] || [ -z "$body" ]; then
  echo "Usage: $0 <remote-path> <text-or-@file>"
  exit 1
fi

curl $flags -X PUT "$server/fs/content?path=$path" \
  --data-binary "$body"
