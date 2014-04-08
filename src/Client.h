#include <string>
#include "NodeList.h"
using namespace std;

typedef int (*OnStateTrigger)(int state, int code);

#define WRITE_BUF 4096
#define READ_BUF 4096
#define HEART_TIME_INTERVAL 12


class MGNet{

private:
	char remote_ip[48];
	int remote_port;
	int listen_port;

	void settingNet();
	void callTrigger(int state, int code);
//	void* write_func(void*);
//	void* read_func(void* arg);


public:
	int heart_check;
	int sock_fd;
	MGNet();
	MGNet(int listen_port, string remote_ip, int remote_port);
	MGNet(string remote_ip, int remote_port);

	void init();
	int set_listen_port(int port);
	int set_remote_addr(string ip, int port);

	void start();
	void stop();
	void send(string msg);

//callback
	void setOnStateTrigger(OnStateTrigger trigger);

	virtual ~MGNet();
};
