# flags="--trace -"
# flags="--verbose"
username=$1
password=$2
outdir=$(pwd)/tmp
ssnfile=$outdir/session.txt
flags="-b $ssnfile -c $ssnfile"

if [ -z "$username" ]; then
  echo "Usage: $0 <username> <password>"
  exit 1
fi

curl $flags http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "$(printf '{"username":"%s","password":"%s"}' "$username" "$password")" 
