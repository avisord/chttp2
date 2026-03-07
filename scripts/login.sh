# FLAGS="--trace -"
# FLAGS="--verbose"
username=$1
password=$2

curl $FLAGS http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "$(printf '{"username":"%s","password":"%s"}' "$username" "$password")"
