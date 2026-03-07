# flags="--trace -"
# flags="--verbose"
flags="-s -i"
username=$1
password=$2
outdir=$(pwd)/tmp
ssnfile=$outdir/session.txt

if [ -z "$username" ]; then
  echo "Usage: $0 <username> <password>"
  exit 1
fi

response=$(curl $flags http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "$(printf '{"username":"%s","password":"%s"}' "$username" "$password")")

session=$(echo "$response" \
  | grep -i "Set-Cookie:" \
  | sed -n 's/Set-Cookie: session=\([^;]*\).*/\1/p')

echo "$session" > $ssnfile

cat $ssnfile
echo "Session stored in session.txt"
