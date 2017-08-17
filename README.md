Pastry is a distributed hash table to store large amount of data effectively and simultaneously provides provision for search
and retrieval of data very efficiently. This is an implementation of the following paper:

A. Rowstron and P. Druschel. Pastry: Scalable, distributed object location and routing for large-scale peer-to-peer systems.
In Proc. IFIP/ACM Middleware 2001, Heidelberg, Germany, Nov. 2001.

How to compile:
1. Must have libssl-dev installed:
sudo apt-get install libssl-dev

2.compile:
g++ -Wall -std=c++11 -pthread pastrydht.cpp -o pastrydht -lcrypto
