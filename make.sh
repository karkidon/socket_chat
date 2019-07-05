#!/bin/bash
echo Compiling server
g++ server.cpp -o server -lcrypto++
echo Compiling client
g++ client.cpp -o client -lcrypto++