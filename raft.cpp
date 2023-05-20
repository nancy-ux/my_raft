#include<bits/stdc++.h>
#include"raft.h"
#include<time.h>
//#include"epoll_server.h"

using namespace std;

//follower接收是否超时 
bool check_heart_beat(raft_node *rf) {//follower 
	double  time_interval;/* 测量一个事件持续的时间*/
	clock_t start=rf->heart_time_s;
	clock_t finish = clock();
	time_interval = (double)(finish - start) / CLOCKS_PER_SEC;
	if(time_interval>=heart_time) { //超时
        cout<<rf->id<<" receive heart beat time out\n";
		return false;
	}
	return true;
}

//leader固定频率发送heart beat 
bool check_heart_beat2(raft_node *rf) {//leader 
	double  time_interval;/* 测量一个事件持续的时间*/
	clock_t start=rf->send_heart;
	clock_t finish = clock();
	time_interval = (double)(finish - start) / CLOCKS_PER_SEC;
	if(time_interval>=heart_freq) { //超时
        cout<<rf->id<<" leader send heart beat\n";
		return false;
	}
	return true;
}

//candidate 选举是否超时 
bool check_select(raft_node * rf){
	double  time_interval;/* 测量一个事件持续的时间*/
	clock_t start=rf->select_time_s;
	clock_t finish = clock();
	time_interval = (double)(finish - start) / CLOCKS_PER_SEC;
	if(time_interval>=select_time) { //超时
        cout<<rf->id<<" select time out\n";
		return false;
	}
	return true;
} 




//成为候选人
bool be_candidate(raft_node * rf) {
	cout<<rf->id<<" be candidate\n";

	//修改自己的信息
	rf->state=1;//修改结点状态
	rf->leader_id='0';//默认没有leader了
	rf->term++;//任期++
	rf->get_vote=1;//自己一票 
	rf->heart_time_s=clock();//更新心跳的时间
}

//成为leader
bool be_leader(raft_node * rf) {
    cout<<rf->id<<" be leadr\n";

	//修改自己的信息
	rf->state=2;	//修改结点状态
	rf->leader_id=rf->id;
	rf->get_vote=0;		//恢复为0 
	rf->heart_time_s=clock();

	for(int i=0;i<3;i++){	//成为leader初始化对全局的把控 
		rf->next_index[i]=rf->commit_index+1;//假装以为所有follower条目和自己一样 follower会发送check fail告知 再对follower进行更新操作
		rf->match_index[i]=rf->len;
	}
}

//成为follower
bool be_follower(raft_node * rf,char id) {//修改自己的信息
    cout<<rf->id<<" is follower\n";
	rf->state=0;//修改结点状态
	rf->leader_id=id;
	rf->get_vote=0; 
}





















