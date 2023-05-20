
#include<map>
#include<vector>
#include"epoll_server.h"
#include"raft.h"
using namespace std;

//编译命令: g++ main.cpp raft.cpp raft.h epoll_server.cpp epoll_server.h -lpthread

int select_time= 1;//选举超时 1s
int heart_time= 10;//心跳检测超时
int heart_freq= 5;//心跳发送频率
int node_num=3;//节点数量

map<char,int> id_port;//存储 id port ip 的对应关系
map<char,string> id_ip;

int main(int argc,char*argv[]) {
	//每个node固定的id和ip port
	id_port['A']=8000;
	id_port['B']=8001;
	id_port['C']=8002;

	id_ip['A']="0.0.0.0";
	id_ip['B']="0.0.0.0";
	id_ip['C']="0.0.0.0";

	//输入参数不正确
	if(argc!=2) {
		cout<<"please give the node id(eg: A、B、C..)\n";
		exit(1);
	}

	
	char id=*argv[1];
	int port=id_port[id];
	raft_node *rf=new raft_node(id,port);//创建结点实例
	epoll_server(rf);
	return 0;
}
