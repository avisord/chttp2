# FLAGS="--trace -"
FLAGS="--verbose"


curl $FLAGS http://localhost:8080/login \
-H "Content-Type: application/json" \
-d "$(printf '{"username":"a","password":"%d"}' "$PASSWORD__")"