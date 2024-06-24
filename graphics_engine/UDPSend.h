/*
 *  UDPSend.h
 *  MySender
 *
 *  Created by Helmut Hlavacs on 10.12.10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once

extern "C" {
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
}



class UDPSend {

public:
	int sock;
	struct sockaddr_in addr;
	unsigned long packetnum;

	UDPSend();
	~UDPSend(){ if(sock) close(sock); };
	void init( char *address, int port );
	int send( char *buffer, int len  );
	void closeSock();
};



