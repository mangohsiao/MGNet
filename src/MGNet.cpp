/*
 * MGNet.cpp
 *
 *  Created on: Apr 8, 2014
 *      Author: xiao
 */

#include "MGNet.h"

#include <string>
#include <deque>
#include <iostream>

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>

namespace mango {

MGNet* MGNet::m_MGNet = NULL;

NodeList *m_list = NULL;

int heart_count = 0;

//global var
pthread_t th_read = 0;
pthread_t th_write = 0;
pthread_t th_heart = 0;
pthread_cond_t cond_w = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx_w = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_h_count = PTHREAD_MUTEX_INITIALIZER;

bool isConnect = false;
pthread_mutex_t mtx_readyRead = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_readyRead = PTHREAD_COND_INITIALIZER;

int read_thread_cmd = -1;
pthread_mutex_t mtx_read_thread = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_read_thread = PTHREAD_COND_INITIALIZER;
int restore_flags;

StateCallback MGNet::st_cb_func = NULL;
ReceiveCallback MGNet::rc_cb_func = NULL;

// thread handler
void* thread_write_run(void* arg);
void* thread_read_run(void* arg);
void* thread_heart_run(void* arg);

void mg_signal_handle(int sig);

int SetRestoreBlock(int fd);
int SetNonBlock(int fd);

int test_thread(pthread_t tid);
int start_read_thread(void* mObj);
int start_write_thread(void* mObj);
int start_heart_thread(void* mObj);
void cleanup_read_thread(void *);

MGNet::MGNet():sock_fd(-1),heart_check(0),heart_break_str(DEFAULT_HEART_STR){
		listen_port = 0;
		strcpy(remote_ip, "125.216.243.243");
		remote_port = 8993;
		init();
}

/*MGNet::MGNet(string remote_ip, int remote_port):sock_fd(0),heart_check(0) {
	this->listen_port = 0;
	strcpy(this->remote_ip, remote_ip.c_str());
	this->remote_port = remote_port;
	init();
}*/

MGNet::~MGNet() {
	delete m_list;
	m_list = NULL;
}

void MGNet::init() {
	if(NULL == m_list)
		m_list = new NodeList();
}

void MGNet::start() {
	initialNet();
	ActReadThreadCmd(1);
}

void MGNet::stop() {
	printf("%s\n", "stop...");
	void* status;
	if(th_read != (unsigned long)0){
		printf("%s = %lu\n", "kill...th_read", th_read);
		pthread_kill(th_read, SIGUSR2);
		pthread_join(th_read,&status);
	}
	if(th_write != (unsigned long)0){
		printf("%s = %lu\n", "kill...th_write", th_write);
		pthread_kill(th_write, SIGUSR2);
		pthread_join(th_write,&status);
	}
	if(th_write != (unsigned long)0){
		printf("%s = %lu\n", "kill...th_heart", th_heart);
		pthread_kill(th_heart, SIGUSR2);
		pthread_join(th_heart,&status);
	}
	close(sock_fd);
	printf("%s\n", "stop...ok");
	call_callback_stat(1992,0);
}

void MGNet::send(string msg) {
	pthread_mutex_lock(&mtx_w);
	if(NULL != m_list){
		m_list->push_back(msg);
	}else{
		call_callback_stat(1,1);
	}
	pthread_cond_signal(&cond_w);
	pthread_mutex_unlock(&mtx_w);
}

void MGNet::initialNet() {
	//setting signal
	int rtvl;
	struct sigaction sig_act;
	memset(&sig_act,0,sizeof(sig_act));
	sig_act.sa_flags = 0;
	sig_act.sa_handler = mg_signal_handle;
	rtvl = sigaction(SIGUSR2,&sig_act,NULL);
	if(rtvl < 0){
		printf("sigaction() < 0\n");
		return;
	}


/*	struct addrinfo *ailist;
	struct addrinfo hint;
	memset(&hint,0,sizeof(hint));
	hint.ai_flags = AI_NUMERICSERV;
	rtvl = getaddrinfo("0.0.0.0","0",&hint,&ailist);
	if(rtvl < 0){
		printf("getaddrinfo() < 0\n");
		return;
	}*/

/*	rtvl = connectNet();
	if(rtvl < 0){
		printf("connect() < 0\n");
		return;
	}*/

	isConnect = false;

	/* read thread */
	rtvl = start_read_thread(this);
	if(rtvl < 0){
		printf("thread 01 error.");
		return;
	}

	/* write thread */
	rtvl = start_write_thread(this);
	if(rtvl < 0){
		printf("thread 02 error.");
		return;
	}

	/* heart break */
	rtvl = start_heart_thread(this);
	if(rtvl < 0){
		printf("thread 03 error.");
		return;
	}

	printf("end\n");
}

int start_read_thread(void* mObj){
	/* read thread */
	return pthread_create(&th_read,NULL,thread_read_run,mObj);
}
int start_write_thread(void* mObj){
	/* read thread */
	return pthread_create(&th_write,NULL,thread_write_run,mObj);
}
int start_heart_thread(void* mObj){
	/* read thread */
	return pthread_create(&th_heart,NULL,thread_heart_run,mObj);
}

void MGNet::setRemoteIp(string remote_ip) {
	strcpy(this->remote_ip, remote_ip.c_str());
}

void MGNet::setRemotePort(int remote_port) {
	this->remote_port = remote_port;
}

void* thread_write_run(void* arg) {
	printf("write_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;
	int _ret;

	char buf[WRITE_BUF];
	while(true){
		pthread_mutex_lock(&mtx_readyRead);
		while(!isConnect){
			pthread_cond_wait(&cond_readyRead,&mtx_readyRead);
		}
		pthread_mutex_unlock(&mtx_readyRead);

		Node* p;
		int len,n;
		pthread_mutex_lock(&mtx_w);
		while(NULL == (p = m_list->pop_front())){
			pthread_cond_wait(&cond_w,&mtx_w);
		}

//		double timeout = 3.0;
//		_ret = arg_ptr->WriteSelect(arg_ptr->sock_fd,timeout);
//		printf("select _ret:%d", _ret);

		if(p != NULL){
			len = p->str.length();
			strcpy(buf,p->str.c_str());
			n = send(arg_ptr->sock_fd,buf,len,0);
			if(n > 0){
				arg_ptr->heart_check = 0;
				printf("send : %d\n", n);
				delete p;
			}else{
				m_list->push_front(p);
				/* set disConnect? */
				pthread_mutex_lock(&mtx_readyRead);
				isConnect = false;
				pthread_mutex_unlock(&mtx_readyRead);
				//TODO callback
				arg_ptr->call_callback_stat(99,98);
			}
		}
		pthread_mutex_unlock(&mtx_w);
	}

	return NULL;
}

void cleanup_read_thread(void *){
	printf("cleaning read thread.\n");
	pthread_mutex_unlock(&mtx_readyRead);
	return;
}

void* thread_read_run(void* arg) {
	printf("read_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;
	pthread_cleanup_push(cleanup_read_thread,NULL);
	int _cmd,_ret = -1;
	while(true){
		pthread_mutex_lock(&mtx_read_thread);
		while(read_thread_cmd < 0){
			pthread_cond_wait(&cond_read_thread,&mtx_read_thread);
		}
		_cmd = read_thread_cmd;
		read_thread_cmd = -1;
		pthread_mutex_unlock(&mtx_read_thread);

		if(_cmd < 0){
			continue;
		}

		//get new _cmd;
		switch(_cmd){
		case 0:
			//test current sock_fd;
//			_ret =
			break;
		case 1:
			//close current sock_fd,and get new one, to connect;
			if(arg_ptr->sock_fd > 0){
				close(arg_ptr->sock_fd);
				arg_ptr->sock_fd = -1;
			}
			_ret = arg_ptr->connectNet();
			break;
		}

		if(_ret < 0){
			arg_ptr->call_callback_stat(1994,1994);
			continue;
		}

		/* connect ok */
		//set isConnect true, notify write and heart thread.

		pthread_mutex_lock(&mtx_readyRead);
		isConnect = true;
		pthread_cond_broadcast(&cond_readyRead);
		pthread_mutex_unlock(&mtx_readyRead);

		//read from sock_fd to buf.
		int n = -1;
		char buf[READ_BUF];
		printf("in read. sock = %d\n", arg_ptr->sock_fd);
		double timeout = 30.0;
		while(arg_ptr->ReadSelect(arg_ptr->sock_fd, timeout) > 0){
			n = recv(arg_ptr->sock_fd, buf, READ_BUF, 0);
			if(n < 0){
				break;
			}
			/* receiving */
			arg_ptr->call_callback_recv(buf,n);
//			write(STDOUT_FILENO, buf, n);
		}
//		if(n < 0){
			printf("recv error.\n");
			pthread_mutex_lock(&mtx_readyRead);
			isConnect = false;
			pthread_mutex_unlock(&mtx_readyRead);
			arg_ptr->call_callback_stat(RECEIVE_ERROR, THREAD_RECEIVE_STOP);
//		}
	}//end of while(true)
	pthread_cleanup_pop(0);
	return NULL;
}

void* thread_heart_run(void* arg) {
	if(arg == NULL){
		return NULL;
	}
	MGNet *arg_ptr = (MGNet*)arg;
	int sleep_sec = HEART_TIME_INTERVAL / 2;
	string heart_str = arg_ptr->get_heart_break_str();
	printf("heart thread = %s\n", heart_str.c_str());
	while(true){
		while(isConnect){
			if(++arg_ptr->heart_check > 1){
//				arg_ptr->send(heart_str);
			}
			sleep(sleep_sec);
		}
		pthread_mutex_lock(&mtx_readyRead);
		pthread_cond_wait(&cond_readyRead,&mtx_readyRead);
		pthread_mutex_unlock(&mtx_readyRead);
	}
	return NULL;
}

void mg_signal_handle(int sig) {
	if(sig!= SIGUSR2){
		printf("NOT SIGUSR2.\n");
		return;
	}
	printf("catch SIGUSR2.\nthread_id = %lu\n", pthread_self());
	pthread_exit((void*)0);
}

int MGNet::reconnectNet() {

	this->ActReadThreadCmd(1);
	return 0;
	//set isConnect false
	int rtvl;
	close(sock_fd);
	sock_fd = -1;
	//set isConnect;
	pthread_mutex_lock(&mtx_readyRead);
	isConnect = false;
	pthread_mutex_unlock(&mtx_readyRead);

	if(0 == test_thread(th_read)){
		rtvl = pthread_kill(th_read,SIGUSR2);
		printf("kill ing ....\n");
		if(rtvl < 0){
			printf("stop read thread failed.\n");
		}else{
			void *status;
			pthread_join(th_read,&status);
			th_read = 0;
			printf("stop read thread ok.\n");
		}
	}
	rtvl = connectNet();
	printf("connectNet RTVL = %d\n", rtvl);
	if(rtvl != 0){
		return rtvl;
	}
	rtvl = start_read_thread(this);
	printf("start read thread RTVL = %d\n", rtvl);
	if(0 != test_thread(th_write)){
		//TODO callbcak
		printf("th_write is stop.");
		start_write_thread(this);
	}
	if(0 != test_thread(th_heart)){
		//TODO callbcak
		printf("th_heart is stop.");
		start_heart_thread(this);
	}
	return 0;
}

int test_thread(pthread_t tid){
	if(tid == 0){
		return -3;
	}
	int rtvl = pthread_kill(tid,0);
	if(rtvl == ESRCH){
		return -1;
	}else if(rtvl == EINVAL){
		return -2;
	}else{
		return 0;
	}
}

int MGNet::disconnectNet() {
	close(sock_fd);
	sock_fd = -1;
	pthread_mutex_lock(&mtx_readyRead);
	isConnect = false;
	pthread_mutex_unlock(&mtx_readyRead);
	return 0;
}

void MGNet::set_heart_break_str(string s) {
	this->heart_break_str = s;
}

string MGNet::get_heart_break_str() {
	return heart_break_str;
}

int MGNet::connectNet() {
	int rtvl;
	printf("ip = %s\nport = %d\n", remote_ip, remote_port);
	struct sockaddr_in sock_addr;
	memset(&sock_addr,0,sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = INADDR_ANY;
	sock_addr.sin_port = 0;

	sock_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock_fd < 0){
		printf("socket < 0\n");
		return -1;
	}else{
		printf("sock_fd = %d\n", sock_fd);
	}

	/* set socket option */
	struct timeval timeout = {HEART_TIME_INTERVAL+20,0};
	rtvl = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));
	if(rtvl < 0){
		printf("setsockopt SO_SNDTIMEO failed.");
	}

//	rtvl = bind(sock_fd, ailist->ai_addr,ailist->ai_addrlen);
	rtvl = bind(sock_fd, (sockaddr*)&sock_addr, sizeof(sock_addr));
	if(rtvl < 0){
		printf("bind < 0\n");
		return -2;
	}

	rtvl = SetNonBlock(sock_fd);
	if(rtvl < 0){
		printf("SetNonBlock < 0\n");
		close(sock_fd);
		sock_fd = -1;
		return -1;
	}

	struct sockaddr_in remote_addr;
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = inet_addr(remote_ip);
	remote_addr.sin_port = htons(remote_port);

	pthread_mutex_lock(&mtx_readyRead);
	isConnect = false;
	pthread_mutex_unlock(&mtx_readyRead);

	printf("begin connect.\n");
	rtvl = connect(sock_fd,(sockaddr*)&remote_addr,sizeof(remote_addr));
	if(rtvl < 0){
		printf("in connect() < 0\t");
		if(errno != EINPROGRESS){
			printf("errno != EINPROGRESS errno = %d\n", errno);
			close(sock_fd);
			sock_fd = -1;
			return -3;
		}
		printf("ing\n");
	}

	if(rtvl == 0){
		printf("connect() == 0\n");
		return sock_fd;
	}

	//poll and wait.
	struct pollfd _connect_client[1];
	int _nfd = 1;
	memset(&_connect_client,0,sizeof(struct pollfd));
	_connect_client[0].fd = sock_fd;
	_connect_client[0].events = POLLOUT | POLLIN;
	int poll_timeout = 10000;	//TODO set timeout.
	rtvl = ::poll(_connect_client, _nfd, poll_timeout);
	printf("poll return : %d\n", rtvl);
	if(rtvl < 0){
		//error.
		close(sock_fd);
		sock_fd = -1;
		return -911;
	}else if(rtvl == 0){
		printf("connect timeout: %d ms\n", poll_timeout);
		close(sock_fd);
		sock_fd = -1;
		return -921;
	}else{
		if((_connect_client[0].revents & POLLIN) || (_connect_client[0].revents & POLLOUT)){
			int error;
			int len = sizeof(error);
			int bok = getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
			if(bok < 0){
				printf("error getsockopt()\n");
				close(sock_fd);
				sock_fd = -1;
				return -910;
			}else if(error){
				printf("error POLL 1, error = %d\n", error);
				close(sock_fd);
				sock_fd = -1;
				return -912;
			}
		}else if((_connect_client[0].revents & POLLERR)
				|| (_connect_client[0].revents & POLLHUP)
				||(_connect_client[0].revents & POLLNVAL))
		{
			printf("error POLL 2\n");
			close(sock_fd);
			sock_fd = -1;
			return -913;
		}
	}

	//connect ok!
	rtvl = SetRestoreBlock(sock_fd);
	if(rtvl < 0){
		close(sock_fd);
		sock_fd = -1;
		printf("SetRestoreBlock error.\n");
	}
	printf("SetRestoreBlock ok.\n");
/*	pthread_mutex_lock(&mtx_readyRead);
	isConnect = true;
	pthread_cond_broadcast(&cond_readyRead);
	pthread_mutex_unlock(&mtx_readyRead);*/

	return 0;
}

inline void MGNet::call_callback_stat(int state, int code) {
	if(NULL != st_cb_func){
		st_cb_func(state,code);
	}
}

inline void MGNet::call_callback_recv(char* buf, int len) {
	if(NULL != rc_cb_func){
		rc_cb_func(buf,len);
	}
}

void MGNet::set_stat_callback(StateCallback st_callback_func) {
	st_cb_func = st_callback_func;
}

void MGNet::set_recv_callback(ReceiveCallback rc_callback_func) {
	rc_cb_func = rc_callback_func;
}

int MGNet::WriteSelect(int fd, double& timeout_sec) {
	struct pollfd _connect_client[1];
	int _nfd = 1;
	memset(&_connect_client,0,sizeof(struct pollfd));
	_connect_client[0].fd = fd;
	_connect_client[0].events = POLLOUT;

	return ::poll(_connect_client,_nfd,(int)(timeout_sec*1000));
}

int MGNet::ReadSelect(int fd, double& timeout_sec) {
	struct pollfd _connect_client[1];
	int _nfd = 1, rtvl;
	memset(&_connect_client,0,sizeof(struct pollfd));
	_connect_client[0].fd = fd;
	_connect_client[0].events = POLLIN;

	printf("in read\n", timeout_sec);
	rtvl = ::poll(_connect_client,_nfd,(int)(timeout_sec*1000));
	if(rtvl < 0){
			//error.
			printf("poll < 0\n", timeout_sec);
		}else if(rtvl == 0){
			printf("read timeout: %f ms\n", timeout_sec);
		}else{
			if((_connect_client[0].revents & POLLIN) || (_connect_client[0].revents & POLLOUT)){
				int error;
				int len = sizeof(error);
				int bok = getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
				if(bok < 0){
					printf("error getsockopt()\n");
					return -910;
				}else if(error){
					printf("error POLL 1, error = %d\n", error);
					return -912;
				}
			}else if((_connect_client[0].revents & POLLERR)
					|| (_connect_client[0].revents & POLLHUP)
					||(_connect_client[0].revents & POLLNVAL))
			{
				printf("error POLL 2\n");
				return -913;
			}
		}
	printf("end read\n", timeout_sec);
}

int SetNonBlock(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	restore_flags = flags;
	if(flags == -1){
		return -1;
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY) == -1)
		return -1;

	return 0;
}

int SetRestoreBlock(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1){
		return -1;
	}
	flags &= ~(O_NONBLOCK|O_NDELAY);
	if(fcntl(fd, F_SETFL, flags) == -1)
		return -1;

	return 0;
}

void MGNet::ActReadThreadCmd(int cmd) {
	pthread_mutex_lock(&mtx_read_thread);
	read_thread_cmd = cmd;
	printf("read_thread_cmd = %d", cmd);
	pthread_cond_broadcast(&cond_read_thread);
	pthread_mutex_unlock(&mtx_read_thread);
}

bool MGNet::checkIsConnect() {
	return isConnect;
}

}
 /* namespace mango */
