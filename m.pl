use strict;
use warnings;

print("compiling ...\n");
system("rm run.exe");
system("g++ data.cpp main.cpp tcp_flow.cpp user.cpp util.cpp -L./libpcap-1.4.0 -lpcap -lsocket -lz -lnsl -I ./libpcap-1.4.0/ -o ./run.exe");

