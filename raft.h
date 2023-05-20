
#ifndef RAFT_H
#define RAFT_H
#include <bits/stdc++.h>
#include <map>
#include <vector>
#include <string.h>
using namespace std;

//声明全局变量
extern int select_time; //选举超时 3s
extern int heart_time;	//心跳检测超时
extern int heart_freq;	//心跳发送频率
extern int node_num;	//节点数量

extern map<char, int> id_port;	//每个结点的id port
extern map<char, string> id_ip; //每个结点的id ip


/*
初始化:列表里有一个空 entry index从0开始, 且已经提交    因此len=1
*/

struct entry
{
	int index;
	int term;
	string commend; //命令
	entry(){
		index=0;
		term=0;
		commend="";
	}
};

class raft_node
{
public:
	char id;  //结点编号  A、B、C
	int port; //结点端口 对应的8000 8001 8002

	int term;			   //当前任期
	int get_vote;		   //本轮获得的票数
	 //结点状态：0——follower  1——candidate   2——leader
	int state;			  
	char leader_id;		   //当前领导结点的id
	clock_t heart_time_s;  //最近获得的心跳时间
	clock_t select_time_s; //最近选举超过时间
	clock_t send_heart;	   //最近发送的心跳时间

	int len;
	entry log_list[50]; // index + term + commend

	// leader发信息有：index term command commit_index

	int commit_index; //应该且可以 提交到的最高日志项索引   这两个来控制自己的提交更新，中间的就是需要进行操作的，对于所有server都适用
	// follower.commit_index=leader.commit_index-1

	int last_applied; //已经提交到了的日志的索引

	// leader: 通过这个来修复日志
	int next_index[3]; //下一条，应该发送给follower的条目索引 初始化为1+leader最后一条日志的索引
	//一致行检查失败就发送自己index，减小next_index重试
	int match_index[3]; // follower上日志条目的索引 初始化位0
	int success;

	// int prev_index; //之前条目的index leader
	// int prev_term;	//之前条目的term    leader

	// int last_index; // candidate最后的日志信息 比较这个信息来决定谁的日志更完整
	// int last_term;

	// int leader_commit_index; // leader已经提交到了的日志索引index
	// bool success;			 // follower包含prev_index和prev_term匹配的项目 true

	raft_node(char ID, int PORT)
	{ //结点初始化
		id = ID;
		port = PORT;
		term = 0;	//任期 初始化为0
		//vote_for = 0;	
		get_vote = 0;	//初始化为0 获得的票数
		state = 0;//状态：
		leader_id = '0'; //表示没有领导者
		heart_time_s = clock();
		select_time_s = clock();
		send_heart = clock();
		success=0;

		len = 1;
		commit_index = 0; //应该且可以提交的最高日志项索引
		last_applied = 0; //已经提交了的最高日志索引
		for (int i = 0; i < 3; i++)
		{
			next_index[i] = 1;	//下一条发送给follower的日志索引
			match_index[i] = 0; // follower的最高日志索引
		}
		for (int i = 0; i < 50; i++)
		{ //初始化为空字串 便于一致性check
			log_list[i].index = log_list[i].term = 0;
			log_list[i].commend = "";
		}
		// prev_index = 0;//leader 发送更新的条目的上一个条目
		// prev_term = 0;// 发送更新的条目的上一个条目

		// last_index = 0;//candidate最后的日志信息 比较这个信息来决定谁的日志更完整
		// last_term = 0;
		//...
	}
};

//计时器初始化
void heart_beat0(raft_node *rf);
//是否超时?
bool check_heart_beat(raft_node *rf);
//成为候选人
bool be_candidate(raft_node *rf);
bool check_select(raft_node *rf);
bool be_leader(raft_node *rf);

bool check_heart_beat2(raft_node *rf);
bool be_follower(raft_node *rf, char id);
void commit_task(raft_node *rf);

#endif
