session=$(cat tmp/session.txt)
server="http://localhost:8080"

if [ -z "$session" ]; then
  echo "Usage: $0 <session_id>"
  exit 1
fi

curl $FLAGS $server/whoami \
  -H "Content-Type: application/json" \
  -H "Cookie: session=$session"
