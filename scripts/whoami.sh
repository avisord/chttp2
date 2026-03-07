server="http://localhost:8080"
outdir=$(pwd)/tmp
ssnfile=$outdir/session.txt
flags="-b $ssnfile -c $ssnfile"


curl $flags $server/whoami \
  -H "Content-Type: application/json" \
