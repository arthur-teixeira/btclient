set -xe;

CFLAGS="-Wall -Wextra -ggdb"
LIBS="`pkg-config --libs openssl`"

mkdir -p bin
clang $CFLAGS *.c -o bin/client $LIBS
