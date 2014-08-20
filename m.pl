use strict;
use warnings;

print("compiling ...\n");
system("rm run");
system("g++ data.cpp main.cpp tcp_flow.cpp user.cpp util.cpp -Wno-deprecated -Llib -lpcap -I ./libpcap-1.3.0/ -o ./run");
