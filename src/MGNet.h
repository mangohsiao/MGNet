/*
 * MGNet.h
 *
 *  Created on: Apr 8, 2014
 *      Author: xiao
 */

#ifndef MGNET_H_
#define MGNET_H_

#include <string>
#include "NodeList.h"
#include "MGNetErr.h"
using namespace std;

typedef int (*StateCallback)(int state, int code);

#define WRITE_BUF 4096
#define READ_BUF 4096
#define HEART_TIME_INTERVAL 12

#define DEFAULT_HEART_STR "#h\n"

namespace mango {

class MGNet {
public:
	int sock_fd;
	int heart_check;

	virtual ~MGNet();

	void setRemoteIp(string remote_ip);
	void setRemotePort(int remote_port);

	void set_heart_break_str(string s);
	string get_heart_break_str();

	void setStateCallback(StateCallback callback_func);
	void init();
	void start();
	void stop();
	int reconnectNet();
	int disconnectNet();
	void send(string msg);
	void call_callback_func(int state, int code);

private:
	char remote_ip[48];
	int remote_port;
	int listen_port;
	string heart_break_str;
	MGNet();
	static MGNet* m_MGNet;
	void initialNet();
	int connectNet();

public:

	static MGNet* ins(){
		if(NULL == MGNet::m_MGNet){
			MGNet::m_MGNet = new MGNet();
		}
		return MGNet::m_MGNet;
	}
	static void release(){
		MGNet::m_MGNet->~MGNet();
	}
};
} /* namespace mango */

#endif /* MGNET_H_ */
