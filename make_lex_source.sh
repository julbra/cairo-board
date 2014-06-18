#/bin/bash

flex -Psan_scanner_ -o san_scanner.c san_scanner.lex;
#sed -i '/#define unput/ d' san_scanner.c;
#sed -i '/static void yyunput.*;$/ d' san_scanner.c;
#sed -i '/static void yyunput/,/^}$/ d' san_scanner.c;
sed -i 's/san_scanner__/san_scanner_/g' san_scanner.c;
gcc -Wall -O2 `pkg-config --cflags gtk+-2.0 cairo librsvg-2.0 gthread-2.0`   -c -o san_scanner.o san_scanner.c
