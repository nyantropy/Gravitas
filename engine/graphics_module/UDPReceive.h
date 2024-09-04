/*
 *  UDPReceive.h
 *  MyReceiver
 *
 *  Created by Helmut Hlavacs on 11.12.10.
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
}


class UDPReceive {
	
public:
	int sock;
	struct sockaddr_in addr;
	char *recbuffer;
	bool leftover;
	
	UDPReceive();
	~UDPReceive(){ delete recbuffer; };
	void init( int port );
	int receive( char *buffer, int len, double *ptime  );
	int receive( char *buffer, int len, char *tag, double *ptime  );
	void closeSock();
};