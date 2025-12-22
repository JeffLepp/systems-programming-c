# http-proxy

A simple HTTP/1.0 web proxy written in C using POSIX sockets.

The proxy listens on a specified port, accepts client connections, forwards HTTP
GET requests to the target server, and relays responses back to the client.

Build:
make

Run:
./proxy <port>

Usage:
Configure a client to use localhost and the specified port as an HTTP proxy.

Tested with:
curl
telnet
netcat
Firefox

# Further Details

`proxy.c`
`csapp.h`
`csapp.c`
    Are starter files. 

`Makefile`
    Makefile that builds the proxy program.

`port-for-user.pl`
    Generates a random port for a particular user
    usage: `./port-for-user.pl <userID>`

`free-port.sh`
    Handy script that identifies an unused TCP port that you can use
    for your proxy or tiny. 
    usage: `./free-port.sh`

`tiny`
    Tiny Web server from the CSAPP textbook

