#!/bin/sh
# End-to-end test for all /fs/* APIs.
# Usage: fstest.sh <username> <password>
set -e

server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile -c $ssnfile"

username=$1
password=$2

if [ -z "$username" ] || [ -z "$password" ]; then
  echo "Usage: $0 <username> <password>"
  exit 1
fi

pass() { printf "\033[32mPASS\033[0m %s\n" "$1"; }
fail() { printf "\033[31mFAIL\033[0m %s: %s\n" "$1" "$2"; exit 1; }
step() { printf "\n\033[1m--- %s ---\033[0m\n" "$1"; }

# ------------------------------------------------------------------ #
step "Login"
resp=$(curl -s $flags -X POST "$server/login" \
  -H "Content-Type: application/json" \
  -d "$(printf '{"username":"%s","password":"%s"}' "$username" "$password")")
echo "$resp"
echo "$resp" | grep -q "Login successful" || fail "login" "$resp"
pass "login"

readonly="-b $ssnfile"

# ------------------------------------------------------------------ #
step "401 without session cookie"
resp=$(curl -s "$server/fs/list?path=.")
echo "$resp"
echo "$resp" | grep -q "Unauthorized" || fail "401 check" "$resp"
pass "401 without cookie"

# ------------------------------------------------------------------ #
step "mkdir: create testdir/sub"
resp=$(curl -s $readonly -X POST "$server/fs/mkdir" \
  -H "Content-Type: application/json" \
  -d '{"path":"testdir/sub"}')
echo "$resp"
echo "$resp" | grep -q "created" || fail "mkdir" "$resp"
pass "mkdir testdir/sub"

# ------------------------------------------------------------------ #
step "write: PUT testdir/hello.txt"
resp=$(curl -s $readonly -X PUT "$server/fs/content?path=testdir/hello.txt" \
  --data-binary "hello world")
echo "$resp"
echo "$resp" | grep -q "saved" || fail "write" "$resp"
pass "write testdir/hello.txt"

# ------------------------------------------------------------------ #
step "read: GET testdir/hello.txt"
resp=$(curl -s $readonly "$server/fs/content?path=testdir/hello.txt")
echo "$resp"
echo "$resp" | grep -q "hello world" || fail "read" "$resp"
pass "read testdir/hello.txt"

# ------------------------------------------------------------------ #
step "stat: GET testdir/hello.txt"
resp=$(curl -s $readonly "$server/fs/stat?path=testdir/hello.txt")
echo "$resp"
echo "$resp" | grep -q '"type":"file"' || fail "stat" "$resp"
pass "stat testdir/hello.txt"

# ------------------------------------------------------------------ #
step "list: GET testdir"
resp=$(curl -s $readonly "$server/fs/list?path=testdir")
echo "$resp"
echo "$resp" | grep -q '"entries"' || fail "list" "$resp"
pass "list testdir"

# ------------------------------------------------------------------ #
step "copy: testdir/hello.txt -> testdir/hello2.txt"
resp=$(curl -s $readonly -X POST "$server/fs/copy" \
  -H "Content-Type: application/json" \
  -d '{"from":"testdir/hello.txt","to":"testdir/hello2.txt"}')
echo "$resp"
echo "$resp" | grep -q "copied" || fail "copy" "$resp"
pass "copy hello.txt -> hello2.txt"

# ------------------------------------------------------------------ #
step "rename: testdir/hello2.txt -> testdir/world.txt"
resp=$(curl -s $readonly -X POST "$server/fs/rename" \
  -H "Content-Type: application/json" \
  -d '{"path":"testdir/hello2.txt","name":"world.txt"}')
echo "$resp"
echo "$resp" | grep -q "renamed" || fail "rename" "$resp"
pass "rename hello2.txt -> world.txt"

# ------------------------------------------------------------------ #
step "move: testdir/world.txt -> world.txt"
resp=$(curl -s $readonly -X POST "$server/fs/move" \
  -H "Content-Type: application/json" \
  -d '{"from":"testdir/world.txt","to":"world.txt"}')
echo "$resp"
echo "$resp" | grep -q "moved" || fail "move" "$resp"
pass "move testdir/world.txt -> world.txt"

# ------------------------------------------------------------------ #
step "upload: POST testdir/sub (multipart)"
tmpfile=$(mktemp /tmp/fstest_upload.XXXXXX)
echo "uploaded content" > "$tmpfile"
resp=$(curl -s $readonly -X POST "$server/fs/upload?path=testdir/sub" \
  -F "file=@$tmpfile;filename=upload.txt")
rm -f "$tmpfile"
echo "$resp"
echo "$resp" | grep -q "uploaded" || fail "upload" "$resp"
pass "upload to testdir/sub"

# ------------------------------------------------------------------ #
step "download: GET testdir/hello.txt"
resp=$(curl -s $readonly "$server/fs/download?path=testdir/hello.txt")
echo "$resp" | head -c 80
echo
echo "$resp" | grep -q "hello world" || fail "download" "$resp"
pass "download testdir/hello.txt"

# ------------------------------------------------------------------ #
step "delete file: world.txt"
resp=$(curl -s $readonly -X DELETE "$server/fs/file?path=world.txt")
echo "$resp"
echo "$resp" | grep -q "deleted" || fail "delete file" "$resp"
pass "delete world.txt"

# ------------------------------------------------------------------ #
step "delete file: testdir/sub/upload.txt"
resp=$(curl -s $readonly -X DELETE "$server/fs/file?path=testdir/sub/upload.txt")
echo "$resp"
echo "$resp" | grep -q "deleted" || fail "delete upload.txt" "$resp"
pass "delete testdir/sub/upload.txt"

# ------------------------------------------------------------------ #
step "delete file: testdir/hello.txt"
resp=$(curl -s $readonly -X DELETE "$server/fs/file?path=testdir/hello.txt")
echo "$resp"
echo "$resp" | grep -q "deleted" || fail "delete hello.txt" "$resp"
pass "delete testdir/hello.txt"

# ------------------------------------------------------------------ #
step "rmdir: testdir/sub"
resp=$(curl -s $readonly -X DELETE "$server/fs/dir?path=testdir/sub")
echo "$resp"
echo "$resp" | grep -q "removed" || fail "rmdir sub" "$resp"
pass "rmdir testdir/sub"

# ------------------------------------------------------------------ #
step "rmdir: testdir"
resp=$(curl -s $readonly -X DELETE "$server/fs/dir?path=testdir")
echo "$resp"
echo "$resp" | grep -q "removed" || fail "rmdir testdir" "$resp"
pass "rmdir testdir"

# ------------------------------------------------------------------ #
printf "\n\033[32;1mAll tests passed.\033[0m\n"
