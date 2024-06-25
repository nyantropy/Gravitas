/*
 *  UDPReceive.cpp
 *  MyReceiver
 *
 *  Created by Helmut Hlavacs on 11.12.10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

//pretty much the same as it originally was, but i added a timeout since it seems to lock up my thread otherwise
#include "UDPReceive.h"

extern "C" {
	#include <stdio.h>
	#include <string.h>
}

#include <iostream>
#include <cerrno>
#include <cstring>

typedef struct RTHeader {
	double		  time;
	unsigned long packetnum;
	unsigned char fragments;
	unsigned char fragnum;
} RTHeader_t;



UDPReceive::UDPReceive() {
	recbuffer = new char[65000];
}


void UDPReceive::init(int port) {
    if (sock) close(sock);

    sock = socket(PF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
}



int UDPReceive::receive( char *buffer, int len, double *ptime ) 
{
	return receive( buffer, len, "", ptime );
}


int UDPReceive::receive(char* buffer, int len, char* tag, double* ptime) 
{
    struct sockaddr_in si_other;
    socklen_t slen = sizeof(si_other);

    RTHeader_t* pheader = (RTHeader_t*)recbuffer;

    bool goon = true;
    int bytes = 0;
    int packetnum = -1;
    int fragments = -1;
    int fragnum = -1;
    int nextfrag = 1;

    while (goon) {
        int ret = 0;

        if (!leftover) 
        {
            ret = recvfrom(sock, recbuffer, 65000, 0, (sockaddr*)&si_other, &slen);
            if (ret == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    std::cerr << "Receive timed out\n";
                } else {
                    std::cerr << "Receive error: " << strerror(errno) << "\n";
                }
                return -1;
            }
        }
        leftover = false;

        if (ret > sizeof(RTHeader_t)) {
            if (packetnum == -1) { //first fragment of the new packet
                packetnum = pheader->packetnum;
            }

            if (packetnum != pheader->packetnum) { //last fragments lost
                std::cerr << "Last Frag " << nextfrag << " lost\n";
                leftover = true;
                return -1;
            }

            if (nextfrag != pheader->fragnum) { //a fragment is missing
                std::cerr << "Fragment " << nextfrag << " lost\n";
                return -1;
            }
            nextfrag++;

            memcpy(buffer + bytes, recbuffer + sizeof(RTHeader_t), ret - sizeof(RTHeader_t));
            bytes += ret - sizeof(RTHeader_t);

            if (pheader->fragments == pheader->fragnum) goon = false; //last fragment

            packetnum = pheader->packetnum;
            fragments = pheader->fragments;
            fragnum = pheader->fragnum;

            *ptime = pheader->time;

        } else {
            std::cerr << "Fragment " << pheader->fragnum << " not larger than " << sizeof(RTHeader_t) << "\n";
            return -1;
        }
    }

    leftover = false;
    return bytes;
}


void UDPReceive::closeSock() {
	close( sock );
}



