# FLAGS="--trace -"
# FLAGS="--verbose"
#SESSION_TOKEN="bcbc86fe8cb3d3fca4e27ea29f4b369b86c8d88434ce63a2296cf2f62839ac56"

curl $FLAGS http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "$(printf '{"username":"b","password":"%s"}' "1234")"
#  -d '{"username":"a","password":"2568"}'
#-H "Cookie: session=$SESSION_TOKEN" \
