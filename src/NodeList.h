/*
 * NodeList.h
 *
 *  Created on: Apr 5, 2014
 *      Author: xiao
 */
#include <string>

using namespace std;

#ifndef NODELIST_H_
#define NODELIST_H_


typedef struct Node{
	string str;
	Node *next;
}Node;


class NodeList {
private:
	Node *head;
	Node *tail;

public:
	NodeList();
	virtual ~NodeList();

//	Node* get_head();
	Node* pop_front();
	Node* push_back(string str);
	Node* push_front(string str);
	Node* push_front(Node* temp);
};

#endif /* NODELIST_H_ */
