# socket_chat

EPOLL based socket chat with [AES](https://www.cryptopp.com/wiki/Advanced_Encryption_Standard) encryption  
  
```
Usage: server [OPTION]  
  -v, --verbose Debug mode ON  
  -p, --port specify port  
  -ip specify ip  
  -u=INT, --userlimit=INT set user limit  
  --gen-key= generate new encryption key
  -h, --help print this and terminate  
  
Usage: client [OPTION]  
  -v, --verbose Debug mode ON  
  -p, --port specify port  
  -ip specify ip  
  -m=INT, --maxmsglen=INT set user limit  
  -h, --help print this and terminate  
```  
  
Available client commands:  
`@online` - list users online  
`@name new_nick_name` - set username as *new_nick_name*. Fails if user with the same name exists.  
`@nickname private message text` - send private message. This message isn't kept in history.
  
Server default settings:  
 Keeps last 10 public messages with names and sends them to newly connected clients.  
 Limits users by 30, new connections rejected after the handshake.  
 Default message limit: 4096 characters.  
 Default port: 44444.  
 Default ip: 127.0.0.1.  

**License:**  
  
This is free and unencumbered software released into the public domain.  
  
Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
  
In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.
  
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
  
For more information, please refer to <http://unlicense.org/>  
  
Encryption library used by this project:  
  
Crypto++® Library 8.2  
  
https://www.cryptopp.com/License.txt  
  
The Crypto++ Library (as a compilation) is currently licensed under the Boost
Software License 1.0 (http://www.boost.org/users/license.html).  
  
Boost Software License - Version 1.0 - August 17th, 2003  
  
