#!/bin/sh

clang -c src/http.c
clang -c src/date_utils.c
clang -c src/mime.c
clang -c src/socket.c

clang src/server.c http.o date_utils.o mime.o socket.o -o bin/server
