//============================================================================
// Name        : MGnet.cpp
// Author      : xiao
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include "MGNet.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <string>
using namespace std;
using namespace mango;

int m_status = 0;

int stat_callback(int status, int code){
	m_status = status;
	cout << "trigger : " << status << endl;
	return status;
}

void recv_callback(char *buf, int len){
	write(STDOUT_FILENO, buf, len);
}

int main() {
//	struct sigaction sig_act;

	MGNet *mNet = MGNet::ins();
	mNet->setRemoteIp("125.216.243.235");
	mNet->setRemotePort(8002);
	mNet->set_heart_break_str("#b\n");
	mNet->set_recv_callback(recv_callback);
	mNet->set_stat_callback(stat_callback);
	mNet->start();


//	MGNet *mNet = new MGNet();
//	MGNet *mNet = new MGNet("125.216.243.235", 8002);
//	mNet->set_listen_port(8991);
//	mNet->set_remote_addr("125.216.243.235",8002);
//	mNet->setStateCallback(callback);
//	mNet->start();
//	mNet->stop();

	if(m_status > 100){
		return m_status;
	}

//	string input;
	char buf[256];
	while(gets(buf)){
		string input(buf);
		if(0 == input.compare("quit")){
			mNet->stop();
			break;
		}else if(0 == input.compare("recon")){
//			mNet->reconnectNet();
			mNet->ActReadThreadCmd(1);
			printf("reconnectNet..\n");
			continue;
		}else if(0 == input.compare("discon")){
//			mNet->disconnectNet();
			printf("disconnect..\n");
			continue;
		}else{
			mNet->send(input + "\n");
		}
	}
	printf("game over.\n");
	return 0;
}
