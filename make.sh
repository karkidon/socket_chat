#!/bin/bash
echo Compiling server
g++ server.cpp -o server
echo Compiling client
g++ client.cpp -o client