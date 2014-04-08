#include "Client.h"

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

/*
class MGNet{
private:
	char remote_ip[48];
	int remote_port;
	int listen_port;

public:
	void init();
	int set_listen_port(int port);
	int set_remote_addr(string ip, int port);

	void start();
	void stop();

	virtual ~MGNet();
};
*/

typedef struct param{
	int sock_fd;
} Param;

//global var
pthread_t th_read = -1;
pthread_t th_write = -1;
pthread_t th_heart = -1;
pthread_cond_t cond_w = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx_w = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_h_count = PTHREAD_MUTEX_INITIALIZER;

OnStateTrigger trigger;

int socket_fd = -1;
NodeList *m_list;
//Param arg_01;

// thread handler
void* write_func(void*);
void* read_func(void*);
void* thread_heart(void* arg);

void mg_signal_handle(int sig);

MGNet::MGNet():sock_fd(-1),heart_check(0){
	trigger = NULL;
	m_list = new NodeList();
	this->listen_port = 0;
	strcpy(this->remote_ip, "125.216.243.243");
	this->remote_port = 8993;
}

MGNet::MGNet(string remote_ip, int remote_port):sock_fd(-1),heart_check(0){
	trigger = NULL;
	m_list = new NodeList();
	this->listen_port = 0;
	strcpy(this->remote_ip, remote_ip.c_str());
	this->remote_port = remote_port;
}

void MGNet::callTrigger(int state, int code){
	if(NULL != trigger){
		trigger(state, code);
	}
}

MGNet::~MGNet(){
	delete m_list;
}

MGNet::MGNet(int listen_port, string remote_ip, int remote_port):sock_fd(-1),heart_check(0){
	trigger = NULL;
	m_list = new NodeList();
	this->listen_port = listen_port;
	strcpy(this->remote_ip, remote_ip.c_str());
	this->remote_port = remote_port;
}

void MGNet::init(){
	//init something;
}

/*int MGNet::set_listen_port(int port){
	this->listen_port = port;
	return 0;
}

int MGNet::set_remote_addr(string ip, int port){
	strcpy(this->remote_ip, ip.c_str());
	this->remote_port = port;
	printf("remote ip: %s\nport: %d\n", this->remote_ip, this->remote_port);
	return 0;
}*/

void MGNet::start(){
	printf("remote ip: %s\nport: %d\n", this->remote_ip, this->remote_port);
	printf("%s\n", "start...");
	settingNet();
}

void MGNet::stop(){
	printf("%s\n", "stop...");
	printf("%s = %lu\n", "kill...th_read", th_read);
	pthread_kill(th_read, SIGUSR1);
	printf("%s = %lu\n", "kill...th_write", th_write);
	pthread_kill(th_write, SIGUSR1);
	printf("%s = %lu\n", "kill...th_heart", th_heart);
	pthread_kill(th_heart, SIGUSR1);
	close(sock_fd);
	void* status;
	pthread_join(th_read,&status);
	pthread_join(th_write,&status);
	pthread_join(th_heart,&status);
	printf("%s\n", "stop...ok");
}


void MGNet::setOnStateTrigger(OnStateTrigger trigger_in){
	trigger = trigger_in;
}

void MGNet::send(string msg){
	pthread_mutex_lock(&mtx_w);
	m_list->push_back(msg);
	pthread_cond_signal(&cond_w);
	pthread_mutex_unlock(&mtx_w);
}


void MGNet::settingNet(){
	//setting signal
	int rc;
	struct sigaction sig_act;
	memset(&sig_act,0,sizeof(sig_act));
	sig_act.sa_flags = 0;
	sig_act.sa_handler = mg_signal_handle;
	rc = sigaction(SIGUSR2,&sig_act,NULL);


	int rtvl;
	struct addrinfo *ailist;
	struct addrinfo hint;
	memset(&hint,0,sizeof(hint));
	hint.ai_flags = AI_NUMERICSERV;

	rtvl = getaddrinfo("0.0.0.0","0",&hint,&ailist);
	if(rtvl < 0){
		printf("getaddrinfo() < 0\n");
		return;
	}

	sock_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock_fd < 0){
		printf("socket < 0\n");
		return;
	}

	rtvl = bind(sock_fd, ailist->ai_addr,ailist->ai_addrlen);
	if(rtvl < 0){
		printf("bind < 0\n");
		return;
	}

	/* read thread */
	rtvl = pthread_create(&th_read,NULL,read_func,this);
	if(rtvl < 0){
		printf("thread 01 error.");
		return;
	}

	/* write thread */
	rtvl = pthread_create(&th_write,NULL,write_func,this);
	if(rtvl < 0){
		printf("thread 02 error.");
		return;
	}

	/* heart break */
	rtvl = pthread_create(&th_heart,NULL,thread_heart,this);
	if(rtvl < 0){
		printf("thread 03 error.");
		return;
	}

	printf("end\n");
}

/*void* thread_input(void*){
	char read_buf[2048];
	while(gets(read_buf)){
		string str(read_buf);
		pthread_mutex_lock(&mtx_w);
		m_list->push_back(str);
		pthread_cond_signal(&cond_w);
		pthread_mutex_unlock(&mtx_w);
	}
	return NULL;
}*/

void* write_func(void* arg){
	printf("write_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;

	int rtvl;
	struct addrinfo *ailist;
	struct addrinfo hint;
	memset(&hint,0,sizeof(hint));
	hint.ai_flags = AI_NUMERICSERV;
	if((rtvl = getaddrinfo("125.216.243.235","8002",&hint,&ailist)) < 0){
		printf("write_func getaddrinfo() < 0\n");
		return NULL;
	}

	int n;
	int sockfd = arg_ptr->sock_fd;
	if((n = connect(sockfd,ailist->ai_addr,ailist->ai_addrlen)) < 0){
		printf("write_func connect() < 0\n");
		return NULL;
	}

	char buf[2048];
	while(true){
		printf("in write()\n");
		Node* p;
		int len,n;
		pthread_mutex_lock(&mtx_w);
		while(NULL == (p = m_list->pop_front())){
			pthread_cond_wait(&cond_w,&mtx_w);
		}

		if(p != NULL){
			p->str.append("\n");
			len = p->str.length();
			strcpy(buf,p->str.c_str());
			n = send(sockfd,buf,len,0);
			if(n > 0){
				arg_ptr->heart_check = 0;
			}
			printf("send : %d\n", n);
			delete p;
		}
		pthread_mutex_unlock(&mtx_w);
	}

	return NULL;
}
void* read_func(void* arg){

	sleep(1);
	printf("read_func start...\n");
	MGNet *arg_ptr = (MGNet*)arg;

	int n,socket_fd;
	char buf[4096];
	socket_fd = arg_ptr->sock_fd;
	while((n = recv(socket_fd, buf, 4096, 0)) > 0){
		write(STDOUT_FILENO, buf, n);
	}
	if(n < 0){
		printf("recv error.\n");
	}
	return NULL;
}

void* thread_heart(void* arg){
	MGNet *arg_ptr = (MGNet*)arg;
	int sleep_sec = HEART_TIME_INTERVAL / 2;
	while(true){
		if(++arg_ptr->heart_check > 1){
			arg_ptr->send("#h");
		}
		sleep(sleep_sec);
	}
	return NULL;
}

void mg_signal_handle(int sig){
	if(sig!= SIGUSR2){
		printf("NOT SIGUSR2.\n");
		return;
	}
	printf("catch SIGUSR2.\nthread_id = %lu\n", pthread_self());
	pthread_exit((void*)0);
}
