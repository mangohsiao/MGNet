/*
 * NodeList.cpp
 *
 *  Created on: Apr 5, 2014
 *      Author: xiao
 */

#include "NodeList.h"
#include <iostream>
using namespace std;

NodeList::NodeList() {
	head = NULL;
	tail = NULL;
}

NodeList::~NodeList() {
	Node *p,*temp;
	p = head;
	while(NULL != head){
		temp = p;
		p = p->next;
		delete temp;
	}
}

Node* NodeList::pop_front(){
	if(NULL != head){
		Node* p = head;
		head = head->next;
		return p;
	}else{
		return NULL;
	}
}

Node* NodeList::push_back(string str){
	if(NULL == head){
		Node *temp = new Node;
		temp->next = NULL;
		temp->str = str;
		head = tail = temp;
		return tail;
	}else{
		Node *temp2 = new Node;
		temp2->next = NULL;
		temp2->str = str;
		tail->next = temp2;
		tail = temp2;
		return tail;
	}
}

Node* NodeList::push_front(string str) {
	Node *temp = new Node;
	temp->str = str;
	if(NULL == head){
		temp->next = NULL;
		head = tail = temp;
	}else{
		temp->next = head;
		head = temp->next;
	}
	return head;
}


Node* NodeList::push_front(Node* temp) {
	if(NULL == head){
			temp->next = NULL;
			head = tail = temp;
		}else{
			temp->next = head;
			head = temp->next;
		}
		return head;
}
/*int main(){
	NodeList m_list;
	string a = "hello,";
	string b = "mango,";
	string c = "luoooo,";

	m_list.push_back(a);
	m_list.push_back(b);
	m_list.push_back(c);
	m_list.push_back(c);
	m_list.push_back(c);
	m_list.push_back(c);
	m_list.push_back(c);

	Node *p;
	while((p = m_list.pop_front())!=NULL){
		cout << p->str << endl;
		delete p;
	}
	return 0;
}*/

