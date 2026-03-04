SESSION="bcbc86fe8cb3d3fca4e27ea29f4b369b86c8d88434ce63a2296cf2f62839ac56"
#SESSION="d694d53ee75c219f0e60406c2795d359ae7537f1185f17e956ea939328589d60"
SERVER="http://localhost:8080"

curl $FLAGS http://localhost:8080/whoami \
  -H "Content-Type: application/json" \
  -H "Cookie: session=$SESSION"
