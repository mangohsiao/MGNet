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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

namespace mango {

MGNet* MGNet::m_MGNet = NULL;

NodeList *m_list = NULL;

//global var
pthread_t th_read = -1;
pthread_t th_write = -1;
pthread_t th_heart = -1;
pthread_cond_t cond_w = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx_w = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_h_count = PTHREAD_MUTEX_INITIALIZER;

bool isConnect = false;
pthread_mutex_t mtx_readyRead = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_readyRead = PTHREAD_COND_INITIALIZER;

StateCallback cb_func = NULL;

// thread handler
void* thread_write_run(void* arg);
void* thread_read_run(void* arg);
void* thread_heart_run(void* arg);

void mg_signal_handle(int sig);


int start_read_thread(void* mObj);

MGNet::MGNet():sock_fd(0),heart_check(0),heart_break_str(DEFAULT_HEART_STR){
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

void MGNet::setStateCallback(StateCallback callback_func) {
	cb_func = callback_func;
}

void MGNet::init() {
	if(NULL == m_list)
		m_list = new NodeList();
}

void MGNet::start() {
	initialNet();
}

void MGNet::stop() {
	printf("%s\n", "stop...");
	void* status;
	if(th_read != (unsigned long)-1){
		printf("%s = %lu\n", "kill...th_read", th_read);
		pthread_kill(th_read, SIGUSR2);
		pthread_join(th_read,&status);
	}
	if(th_write != (unsigned long)-1){
		printf("%s = %lu\n", "kill...th_write", th_write);
		pthread_kill(th_write, SIGUSR2);
		pthread_join(th_write,&status);
	}
	if(th_write != (unsigned long)-1){
		printf("%s = %lu\n", "kill...th_heart", th_heart);
		pthread_kill(th_heart, SIGUSR2);
		pthread_join(th_heart,&status);
	}
	close(sock_fd);
	printf("%s\n", "stop...ok");
}

void MGNet::send(string msg) {
	pthread_mutex_lock(&mtx_w);
	if(NULL != m_list){
		m_list->push_back(msg);
	}else{
		call_callback_func(1,1);
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

	rtvl = connectNet();
	if(rtvl < 0){
		printf("connect() < 0\n");
		return;
	}

	/* read thread */
	rtvl = pthread_create(&th_read,NULL,thread_read_run,this);
	if(rtvl < 0){
		printf("thread 01 error.");
		return;
	}

	/* write thread */
	rtvl = pthread_create(&th_write,NULL,thread_write_run,this);
	if(rtvl < 0){
		printf("thread 02 error.");
		return;
	}

	/* heart break */
	rtvl = pthread_create(&th_heart,NULL,thread_heart_run,this);
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

void MGNet::setRemoteIp(string remote_ip) {
	strcpy(this->remote_ip, remote_ip.c_str());
}

void MGNet::setRemotePort(int remote_port) {
	this->remote_port = remote_port;
}

void MGNet::call_callback_func(int state, int code) {
	if(NULL != cb_func){
		cb_func(state,code);
	}
}

void* thread_write_run(void* arg) {
	printf("write_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;

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
				//TODO callback
			}
		}
		pthread_mutex_unlock(&mtx_w);
	}

	return NULL;
}

void* thread_read_run(void* arg) {
	printf("read_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;

	while(true){
		pthread_mutex_lock(&mtx_readyRead);
		while(!isConnect){
			pthread_cond_wait(&cond_readyRead,&mtx_readyRead);
		}
		pthread_mutex_unlock(&mtx_readyRead);

//		int n,socket_fd;
		int n;
		char buf[READ_BUF];
//		socket_fd = arg_ptr->sock_fd;
		printf("in read. sock = %d\n", arg_ptr->sock_fd);
		while((n = recv(arg_ptr->sock_fd, buf, 4096, 0)) > 0){
			write(STDOUT_FILENO, buf, n);
		}
		if(n < 0){
			printf("recv error.\n");
			arg_ptr->call_callback_func(RECEIVE_ERROR, THREAD_RECEIVE_STOP);
			pthread_mutex_lock(&mtx_readyRead);
			isConnect = false;
			pthread_mutex_unlock(&mtx_readyRead);
		}
	}
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
				arg_ptr->send(heart_str);
			}
			sleep(sleep_sec);
		}
		pthread_mutex_lock(&mtx_readyRead);
		pthread_cond_wait(&cond_readyRead,&mtx_readyRead);
		pthread_mutex_unlock(&mtx_readyRead);
		printf("heart thread over mutex.\n");
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
	//set isConnect false
	int rtvl;
	close(sock_fd);
	sock_fd = -1;
	rtvl = pthread_kill(th_read,SIGUSR2);
	if(rtvl < 0){
		printf("stop read thread failed.\n");
	}else{
		void *status;
		pthread_join(th_read,&status);
	}
	start_read_thread(this);
	connectNet();
	return 0;
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

//	rtvl = bind(sock_fd, ailist->ai_addr,ailist->ai_addrlen);
	rtvl = bind(sock_fd, (sockaddr*)&sock_addr, sizeof(sock_addr));
	if(rtvl < 0){
		printf("bind < 0\n");
		return -2;
	}

	struct sockaddr_in remote_addr;
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = inet_addr(remote_ip);
	remote_addr.sin_port = htons(remote_port);

	rtvl = connect(sock_fd,(sockaddr*)&remote_addr,sizeof(remote_addr));
	if(rtvl < 0){
		printf("in connect() < 0\n");
		pthread_mutex_lock(&mtx_readyRead);
		isConnect = false;
		pthread_mutex_unlock(&mtx_readyRead);
		return -3;
	}

	printf("connect() ok\n");
	pthread_mutex_lock(&mtx_readyRead);
	isConnect = true;
//	pthread_cond_signal(&cond_readyRead);
	pthread_cond_broadcast(&cond_readyRead);
	pthread_mutex_unlock(&mtx_readyRead);

	return 0;
}

} /* namespace mango */
