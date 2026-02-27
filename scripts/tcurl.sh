# FLAGS="--trace -"
FLAGS="--verbose"


curl $FLAGS http://localhost:8080/login \
-H Content-Type: application/json \
-d '{"username":"b","password":"1234"}'