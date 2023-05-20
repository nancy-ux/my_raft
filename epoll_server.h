#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include<bits/stdc++.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <ctype.h>
#include<map>
#include"raft.h"

#define max_size 100
using namespace std;



void  epoll_server(raft_node *rf) ;
int create_listen_fd(int port, int epfd,int ip) ;
void my_accept(int listenfd,int epfd) ;
void hander(raft_node *rf, int cfd, int epfd);
string search_one_course(raft_node *rf,string cou_id);
string search_one_stu(raft_node *rf, string cou_id);

bool give_vote(raft_node * rf,char id,int term ) ;//给别人发送自己的投票给谁
//选举 
bool my_select(raft_node *rf);
void send_heart_beat(raft_node *rf);
int create_TCP(char *buf,raft_node* rf,char it);
void *my_send(void *args,char *buf);
void disconnect(int cfd, int epfd) ;
int ip_int(string temp_ip);
int hexit(char c);
void decode_str(char *to, char *from);
void send_entry(raft_node *rf,char it) ;
#endif