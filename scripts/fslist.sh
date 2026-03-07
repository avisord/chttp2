server="http://localhost:8080"
ssnfile=$(pwd)/tmp/session.txt
flags="-b $ssnfile"
pathp=${1:-.}

curl $flags "$server/fs/list?path=$pathp" \
  -H "Content-Type: application/json"
