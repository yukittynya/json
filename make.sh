rm -rvf build >/dev/null
mkdir -p build/bin

clang -O3 -c test/test.c -o build/test.o
clang -O3 -c src/json.c -o build/json.o
clang -O3 build/test.o build/json.o -o build/bin/test

./build/bin/test
