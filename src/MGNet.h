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
typedef void (*ReceiveCallback)(char *buf, int len);

#define WRITE_BUF 4096
#define READ_BUF 4096
#define HEART_TIME_INTERVAL 24

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

	void init();
	void start();
	void stop();
	int reconnectNet();
	int disconnectNet();
	void send(string msg);

private:
	char remote_ip[48];
	int remote_port;
	int listen_port;
	string heart_break_str;
	MGNet();
	static MGNet* m_MGNet;
	void initialNet();

public:
	static StateCallback st_cb_func;
	void call_callback_stat(int state, int code);
	static ReceiveCallback rc_cb_func;
	void call_callback_recv(char *buf, int len);
	void set_stat_callback(StateCallback st_cb_func);
	void set_recv_callback(ReceiveCallback rc_cb_func);
	int WriteSelect(int fd, double& timeout_sec);
	void ActReadThreadCmd(int cmd);
	int connectNet();
	bool checkIsConnect();
	int ReadSelect(int fd, double& timeout_sec);

	static MGNet* ins(){
		if(NULL == MGNet::m_MGNet){
			m_MGNet = new MGNet();
		}
		return m_MGNet;
	}
	static void release(){
		m_MGNet->~MGNet();
	}
};
} /* namespace mango */

#endif /* MGNET_H_ */
