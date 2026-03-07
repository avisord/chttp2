#!/bin/sh
# Run all /fs/* API scripts in sequence with real args.
# Must be called from the repo root.
# Usage: scripts/runtests.sh <username> <password>

username=$1
password=$2

if [ -z "$username" ] || [ -z "$password" ]; then
  echo "Usage: $0 <username> <password>"
  exit 1
fi

# Always run from repo root so $(pwd)/tmp/session.txt resolves correctly.
cd "$(dirname "$0")/.."

S=./scripts
UPLOAD_FILE=./tmp/test_upload.txt
DOWNLOAD_OUT=/tmp/fstest_downloaded.txt

pass() { printf "\033[32mPASS\033[0m  %s\n" "$1"; }
fail() { printf "\033[31mFAIL\033[0m  %s\n  response: %s\n" "$1" "$2"; FAILURES=$((FAILURES+1)); }
step() { printf "\n\033[1m=== %s ===\033[0m\n" "$1"; }
FAILURES=0

# Check the upload fixture exists
if [ ! -f "$UPLOAD_FILE" ]; then
  echo "Missing upload fixture: $UPLOAD_FILE"
  exit 1
fi

# ------------------------------------------------------------------ #
step "Login"
resp=$(sh "$S/login.sh" "$username" "$password" 2>&1)
echo "$resp"
echo "$resp" | grep -q "Login successful" && pass "login" || fail "login" "$resp"

# Abort early — all other calls will 401 without a valid session.
if echo "$resp" | grep -qv "Login successful"; then
  :
fi

# ------------------------------------------------------------------ #
step "401 guard (no cookie)"
resp=$(curl -s http://localhost:8080/fs/list?path=. 2>&1)
echo "$resp"
echo "$resp" | grep -q "Unauthorized" && pass "401 without cookie" || fail "401 without cookie" "$resp"

# ------------------------------------------------------------------ #
step "mkdir: testdir"
resp=$(sh "$S/fsmkdir.sh" testdir 2>&1)
echo "$resp"
echo "$resp" | grep -q "created" && pass "mkdir testdir" || fail "mkdir testdir" "$resp"

step "mkdir: testdir/sub"
resp=$(sh "$S/fsmkdir.sh" testdir/sub 2>&1)
echo "$resp"
echo "$resp" | grep -q "created" && pass "mkdir testdir/sub" || fail "mkdir testdir/sub" "$resp"

# ------------------------------------------------------------------ #
step "write: testdir/notes.txt"
resp=$(sh "$S/fswrite.sh" testdir/notes.txt "hello from runtests" 2>&1)
echo "$resp"
echo "$resp" | grep -q "saved" && pass "write testdir/notes.txt" || fail "write testdir/notes.txt" "$resp"

# ------------------------------------------------------------------ #
step "read: testdir/notes.txt"
resp=$(sh "$S/fsread.sh" testdir/notes.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "hello from runtests" && pass "read testdir/notes.txt" || fail "read testdir/notes.txt" "$resp"

# ------------------------------------------------------------------ #
step "stat: testdir/notes.txt"
resp=$(sh "$S/fsstat.sh" testdir/notes.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q '"type":"file"' && pass "stat testdir/notes.txt" || fail "stat testdir/notes.txt" "$resp"

# ------------------------------------------------------------------ #
step "list: testdir"
resp=$(sh "$S/fslist.sh" testdir 2>&1)
echo "$resp"
echo "$resp" | grep -q '"entries"' && pass "list testdir" || fail "list testdir" "$resp"

# ------------------------------------------------------------------ #
step "copy: testdir/notes.txt -> testdir/notes_copy.txt"
resp=$(sh "$S/fscopy.sh" testdir/notes.txt testdir/notes_copy.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "copied" && pass "copy notes.txt -> notes_copy.txt" || fail "copy" "$resp"

# ------------------------------------------------------------------ #
step "rename: testdir/notes_copy.txt -> notes_renamed.txt  (same dir)"
resp=$(sh "$S/fsrename.sh" testdir/notes_copy.txt notes_renamed.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "renamed" && pass "rename notes_copy.txt -> notes_renamed.txt" || fail "rename" "$resp"

# ------------------------------------------------------------------ #
step "move: testdir/notes_renamed.txt -> testdir/sub/notes_renamed.txt"
resp=$(sh "$S/fsmove.sh" testdir/notes_renamed.txt testdir/sub/notes_renamed.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "moved" && pass "move notes_renamed.txt -> testdir/sub/" || fail "move" "$resp"

# ------------------------------------------------------------------ #
step "upload: $UPLOAD_FILE -> testdir"
resp=$(sh "$S/fsupload.sh" "$UPLOAD_FILE" testdir 2>&1)
echo "$resp"
echo "$resp" | grep -q "uploaded" && pass "upload test_upload.txt -> testdir/" || fail "upload" "$resp"

# ------------------------------------------------------------------ #
step "download: testdir/notes.txt -> $DOWNLOAD_OUT"
resp=$(sh "$S/fsdownload.sh" testdir/notes.txt "$DOWNLOAD_OUT" 2>&1)
if [ -f "$DOWNLOAD_OUT" ] && grep -q "hello from runtests" "$DOWNLOAD_OUT"; then
  pass "download testdir/notes.txt"
else
  fail "download testdir/notes.txt" "$(cat "$DOWNLOAD_OUT" 2>/dev/null)"
fi
rm -f "$DOWNLOAD_OUT"

# ------------------------------------------------------------------ #
step "stat: testdir  (directory)"
resp=$(sh "$S/fsstat.sh" testdir 2>&1)
echo "$resp"
echo "$resp" | grep -q '"type":"dir"' && pass "stat testdir (dir)" || fail "stat testdir" "$resp"

# ------------------------------------------------------------------ #
step "list: .  (home dir)"
resp=$(sh "$S/fslist.sh" . 2>&1)
echo "$resp"
echo "$resp" | grep -q '"entries"' && pass "list home dir" || fail "list home dir" "$resp"

# ------------------------------------------------------------------ #
# Cleanup — delete files, then dirs bottom-up
step "Cleanup"

resp=$(sh "$S/fsdelfile.sh" testdir/notes.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "deleted" && pass "delete testdir/notes.txt" || fail "delete testdir/notes.txt" "$resp"

resp=$(sh "$S/fsdelfile.sh" testdir/test_upload.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "deleted" && pass "delete testdir/test_upload.txt" || fail "delete testdir/test_upload.txt" "$resp"

resp=$(sh "$S/fsdelfile.sh" testdir/sub/notes_renamed.txt 2>&1)
echo "$resp"
echo "$resp" | grep -q "deleted" && pass "delete testdir/sub/notes_renamed.txt" || fail "delete testdir/sub/notes_renamed.txt" "$resp"

resp=$(sh "$S/fsrmdir.sh" testdir/sub 2>&1)
echo "$resp"
echo "$resp" | grep -q "removed" && pass "rmdir testdir/sub" || fail "rmdir testdir/sub" "$resp"

resp=$(sh "$S/fsrmdir.sh" testdir 2>&1)
echo "$resp"
echo "$resp" | grep -q "removed" && pass "rmdir testdir" || fail "rmdir testdir" "$resp"

# ------------------------------------------------------------------ #
printf "\n"
if [ "$FAILURES" -eq 0 ]; then
  printf "\033[32;1mAll tests passed.\033[0m\n"
else
  printf "\033[31;1m%d test(s) failed.\033[0m\n" "$FAILURES"
  exit 1
fi
