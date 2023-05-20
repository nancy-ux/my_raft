#include "epoll_server.h"
using namespace std;

void epoll_server(raft_node *rf)
{
	int epfd = epoll_create(max_size); //打开一个epoll文件描述符
	if (epfd == -1)
	{
		perror("epoll_create error"); //打开失败
		exit(1);
	}
	int ip = ip_int(id_ip[rf->id]);
	int lfd = create_listen_fd(rf->port, epfd, ip); //监听端口port
	struct epoll_event all[max_size];				//同时能监听的事件
	rf->heart_time_s == clock();					//开始计时
	int f1 = (int(rf->id) - int('A') + 1) % 3;		// A 1 B 2 C 0
	int f2 = (int(rf->id) - int('A') + 2) % 3;		// A 2 B 0 C 1
	while (1)
	{
		if (rf->state == 0 && check_heart_beat(rf) == false)
		{					  //如果是follower 心跳超时了
			be_candidate(rf); //成为候选者 更新作为候选者的信息
			my_select(rf);	  //广播请别人投票的信息 开始选举
		}
		if (rf->state == 1 && rf->get_vote > floor(node_num / 2)) //如果是candidate 自己获票大于一半
		{
			be_leader(rf);		 //成为leader
			send_heart_beat(rf); //广播heart beat
		}
		if (rf->state == 1 && check_select(rf) == false) //是candidate 选举超时
		{
			be_follower(rf, '0'); //成为follower 没有leader
		}
		if (rf->state == 2 && check_heart_beat2(rf) == false) // leader 超时 该发心跳了
		{
			send_heart_beat(rf);
			rf->send_heart = clock();
		}

		if (rf->commit_index > rf->last_applied)
		{
			for (int i = rf->last_applied; i < rf->commit_index; i++)
			{
				cout << "做任务 from " << rf->last_applied << " to " << rf->commit_index << "\n";
				commit_task(rf);	// commit_com();//做任务 执行提交
				rf->last_applied++;
				cout << "完成任务 now:" << rf->commit_index << "\n";
			}
		}

		if (rf->leader_id != '0' && rf->state == 0 && rf->success == 0)
		{ // check_fail 给leader发消息
			char buf[256] = {0};
			sprintf(buf, "%s %c %d", "check_fail", rf->id, rf->len);
			create_TCP(buf, rf, rf->leader_id); //发送给leader
			rf->success = 1;
		}
		if (rf->state == 2 && (rf->next_index[f1] > rf->match_index[f1] + 1 || rf->next_index[f1] < rf->commit_index + 1)) // match是follower的条目数  next是下一条发送的条目
		{
			char it = char('A' + f1);
			cout << "follower" << it << "需要更新follower条目\n";
			send_entry(rf, it);
		}
		if (rf->state == 2 && (rf->next_index[f2] > rf->match_index[f2] + 1 || rf->next_index[f2] < rf->commit_index + 1)) // match是follower的条目数  next是下一条发送的条目
		{
			char it = char('A' + f2);
			cout << "follower" << it << "需要更新follower条目\n";
			send_entry(rf, it);
		}
		int wait_count = epoll_wait(epfd, all, max_size, 0); //非阻塞
		if (wait_count == -1)
		{
			perror("epoll_wait error");
			exit(1);
		}
		int i = 0;
		for (i = 0; i < wait_count; i++)
		{
			struct epoll_event *pev = &all[i];
			if (!(pev->events & EPOLLIN)) //不是读事件
				continue;
			if (pev->data.fd == lfd) // listen事件触发 连接
				my_accept(lfd, epfd);
			else
			{
				hander(rf, pev->data.fd, epfd); //接收报文 进行处理
			}
		}
	}
}

void hander(raft_node *rf, int cfd, int epfd)
{
	// recive(message);
	char buffer[4096];
	int bytes_received = 0;
	fd_set read_fds;					//定义读文件描述符集合
	struct timeval timeout;				//定义超时 时间结构体
	int flags = fcntl(cfd, F_GETFL, 0); // 将 client_fd 设置为非阻塞模式
	fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

	while (1)
	{
		FD_ZERO(&read_fds);		// 清空文件描述符集合
		FD_SET(cfd, &read_fds); // 将 client_fd 加入到文件描述符集合中
		timeout.tv_sec = 5;		// 设置超时时间为 5 秒
		timeout.tv_usec = 0;
		int ret = select(cfd + 1, &read_fds, NULL, NULL, &timeout);
		if (ret < 0) // select 函数出错
		{
			disconnect(cfd, epfd);
			cout << "Failed to select on socket\n";
			return;
		}
		else if (ret == 0) // 超时，没有数据可读
		{
			disconnect(cfd, epfd); // 关闭套接字, cfd从epoll上del
			// cout << "no data...the clinet clsoes the connect\n";
			return;
		}
		memset(buffer, 0, 4096);
		bytes_received = read(cfd, buffer, sizeof(buffer)); // 有数据可读，进行读取操作
		if (bytes_received <= 0)
		{
			disconnect(cfd, epfd);
			// cout << "Failed to read from socket\n";
			return;
		}
		char buf[4096] = {0};					   // 发送数据缓冲区
		if (strncasecmp("select", buffer, 6) == 0) //接收到select的消息
		{
			//解析报文
			char _type[15], _term[1000], _id[1];
			sscanf(buffer, "%[^ ] %[^ ] %[^ ]", _type, _term, _id);
			int term = atoi(_term);
			char id = _id[0];

			//比较term 决定是否更新
			if (term > rf->term) 
			{
				rf->term = term;
				rf->heart_time_s = clock(); //收到选举信息，更新心跳时间
				// cout << "konw select thing,try to give note to " << id << "\n";

				//给别人投票
				sprintf(buf, "%s %d %c\r\n", "give_vote", rf->term, rf->id);

				//发送给给自己发送select的人
				create_TCP(buf, rf, id); 
				//	cout << " send vote over" << endl;
			}
		}
		else if (strncasecmp("give_vote", buffer, 9) == 0) //有人给自己投票
		{
			// cout <<rf->id<< "get vote\n";
			rf->get_vote++; //获得票
		}
		else if (strncasecmp("heart_beat", buffer, 10) == 0)
		{								
			rf->heart_time_s = clock(); //更新心跳时间
			// cout << "get heart_beat\n";

			//解析报文
			char _type[15], _term[1000], _id[1];
			sscanf(buffer, "%[^ ] %[^ ] %[^ ]", _type, _term, _id);
			int term = atoi(_term);//发送heart_beat的结点的term
			char id = _id[0];//发送heart_beat的结点的id  用于follower记录leader_id

			if (term >= rf->term) //比较term 决定是否更新 成为follower
			{
				rf->term = term;
				be_follower(rf, id);
			}
		}
		else if (strncasecmp("entry", buffer, 5) == 0) //面对leader发送的事件
		{											   // entry leader_term index term command commit_index

			//报文格式解析
			int l_term, index, term, prev_term, prev_index, commit_index;
			char _type[6], _l_term[5], _index[5], _term[10], _com[35], _commit_index[5], _prev_term[5], _prev_index[5], _prev_com[30];
			sscanf(buffer, "%[^ ] %[^ ] %[^ ] %[^ ] %[^ ] %[^ ] %[^ ] %[^ ] %[^ ]", _type, _l_term, _index, _term, _com, _commit_index, _prev_index, _prev_term, _prev_com);
			l_term = atoi(_l_term);
			index = atoi(_index);
			term = atoi(_term);
			prev_index = atoi(_prev_index);
			prev_term = atoi(_prev_term);
			commit_index = atoi(_commit_index);
			string com = _com;
			string prev_com = _prev_com;

			cout << "收到entry\n";

			printf("l_term=%d,index=%d,term=%d,prev_index=%d,prev_term=%d,commit_index=%d \n",
				   l_term, index, term, prev_index, prev_term, commit_index);

			//只接受term比自己新的
			if (l_term >= rf->term)
			{
				cout << "check\n";
				if (prev_index != rf->len - 1)
				{ //如果prev_index!=last_index
				  //发送更新 match=len, next_ --  的报文
					//	printf("%s:\n  prev_term=%d,prev_index=%d,rf->len-1=%d\n",
					//		   "check_fail", prev_term, prev_index, rf->len - 1);
					cout << "check fail\n";
					rf->len = min(rf->len, prev_index + 1); // follower 日志回退
					rf->success = 0;
					// sprintf(buf, "%s %c %d", "check_fail", rf->id, rf->len);
				}
				else if (prev_term != rf->log_list[prev_index].term) //如果prev_index=last_index 但是term不一样
				{
					//一致性检查失败
					cout << " check fail,but have fixed!\n";
					printf("prev_term=%d,prev_index=%d,rf->log_list[prev_index].term=%d\n",
						   prev_term, prev_index, rf->log_list[prev_index].term);
					rf->log_list[prev_index - 1].term = term; //命令也要重写
					rf->log_list[prev_index - 1].commend = "11";

					cout << " check ok! 更新条目\n";
					rf->log_list[rf->len].index = index;
					rf->log_list[rf->len].term = term;
					rf->log_list[rf->len].commend = prev_com;
					rf->len++;
					rf->commit_index = min(rf->len, commit_index);
				}
				else
				{ //操作
					cout << " check ok! 更新条目\n";
					rf->log_list[rf->len].index = index;
					rf->log_list[rf->len].term = term;
					rf->log_list[rf->len].commend = com;
					if (index != 0) //如果不是空entry
						rf->len++;
					rf->commit_index = min(rf->len, commit_index);
				}
			}
		}
		else if (strncasecmp("check_fail", buffer, 10) == 0)
		{
			cout << "收到check_fail,更新next_index和match\n";
			char type[15], _id[1], _len[5];
			sscanf(buffer, "%[^ ] %[^ ] %[^ ]", type, _id, _len);
			int len = atoi(_len);
			char id = _id[0];
			int ff = (int(id) - int('A') + 3) % 3; //对应节点的情况
			rf->match_index[ff] = len;			   // 1
			rf->next_index[ff] = len;			   //直接回退到len试试
												   // printf("ff=%d,match_index=%d,next_index=%d\n", ff, rf->match_index[ff], rf->next_index[ff]);
		}

		else //客户端的信息
		{
			if (rf->state == 2) //自己是leader
			{
				//处理buffer，将信息发送给其他结点，等待其他结点相应 返回数据写到buf里
				if (strncasecmp("search_cou/all", buffer, 14) == 0)
				{
					string path = "data";
					char id = rf->id;
					string filename = "/courses.txt";
					string file = path + id + filename;
					ifstream fin(file);
					string s;
					while (getline(fin, s)) //太多信息 只能一部份一部分发
					{
						char temp[100];
						strcpy(temp, s.c_str());
						decode_str(temp, temp); //中文乱码问题
						sprintf(buf, "%s\n", temp);
						// << buf;
						int sum = strlen(buf);
						while (sum > 0)
						{
							int n = send(cfd, buf, strlen(buf), 0);
							sum -= n;
							// cout << sum << endl;
						}
						memset(buf, 0, strlen(buf));
						s.clear();
					}
					fin.close();
				}
				else if (strncasecmp("search_cou", buffer, 10) == 0)
				{
					string s = buffer;
					string cou_id = s.substr(11, strlen(buffer) - 12); // pos len 去除最后的\n
					// cout << cou_id << endl;
					string mess = search_one_course(rf, cou_id);
					mess.substr(0, mess.length() - 1); //去除\n
					char temp[100];
					strcpy(temp, mess.c_str());
					decode_str(temp, temp); //中文乱码问题
					sprintf(buf, "%s", temp);
					//  cout<<"buf:"<<buf<<endl;
					int sum = strlen(buf);
					while (sum > 0)
					{
						int n = send(cfd, buf, strlen(buf), 0);
						sum -= n;
					}
				}
				else if (strncasecmp("search_stu", buffer, 10) == 0)
				{
					string s = buffer;
					string stu_id = s.substr(11, strlen(buffer) - 12); // pos len
					// cout << stu_id << endl;
					string mess = search_one_stu(rf, stu_id);
					mess.substr(0, mess.length() - 1); //去除\n
					char temp[100];
					strcpy(temp, mess.c_str());
					decode_str(temp, temp); //中文乱码问题
					sprintf(buf, "%s", temp);
					int sum = strlen(buf);
					while (sum > 0)
					{
						int n = send(cfd, buf, strlen(buf), 0);
						sum -= n;
					}
				}
				else if (strncasecmp("choose", buffer, 6) == 0)
				{
					string s = buffer;
					string stu_id = s.substr(7, 12);
					string cou_id = s.substr(20, strlen(buffer) - 21); //获得学生id 和课程id
																	   //	cout << stu_id << " " << cou_id << endl;
					string mess = search_one_stu(rf, stu_id);		   //查看是否有错误
					string mess2 = search_one_course(rf, cou_id);
					if ("error" == mess.substr(0, 5) || "error" == mess2.substr(0, 5)) //命令不对
					{
						mess = "stu_id or course error";
						// cout << "stu_id or course error" << endl;
						sprintf(buf, "%s", mess.c_str());
						int sum = strlen(buf);
						while (sum > 0)
						{
							int n = send(cfd, buf, strlen(buf), 0);
							sum -= n;
						}
					}
					else //命令ok
					{
						cout << "id ok\n";
						// stu_id  cou_id都没问题
						// entry index term command commit_index  //加入entry  命令 票数 等待

						int index = rf->len; // index是从1算起
						int term = rf->term;
						string com = s.substr(0, strlen(buffer) - 1); //去除\n
						rf->log_list[rf->len].index = index;		  //加入新的条目
						rf->log_list[rf->len].term = term;
						rf->log_list[rf->len].commend = com;
						int count = 1; //自己一票
						for (int i = 0; i < node_num; i++)
						{
							char it = 'A';
							it = char(it + i);
							if (it == rf->id)
								continue;
							memset(buf, 0, strlen(buf));
							sprintf(buf, "%s", "is_connect?\n");
							int x = create_TCP(buf, rf, it); //发送给除了自己的每个人
							if (x == 0)						 //如果消息正常发送 就暂且认为ok了
								count++;
						}

						if (count > floor(node_num / 2))
						{
							//执行提交任务 更新自己参数噢
							rf->len++;			//任务成功才保留这个条目
							rf->commit_index++; //应该提交的index  1
							cout << "我的commit_index=" << rf->commit_index << ". 给follower发送entry\n";
							for (int i = 0; i < node_num; i++) //发送给除了自己的每个人
							{
								char it = 'A';
								it = char(it + i);
								if (it == rf->id)
									continue;
								send_entry(rf, it);
							}

							memset(buf, 0, strlen(buf)); //发送给客户端说自己 ok了
							sprintf(buf, "%s", "Done!");
							int sum = strlen(buf);
							while (sum > 0)
							{
								int n = send(cfd, buf, strlen(buf), 0);
								sum -= n;
							}
						}
						else
						{
							rf->log_list[rf->len].index = rf->log_list[rf->len].term = 0; //清空

							memset(buf, 0, strlen(buf));
							sprintf(buf, "%s", "busy! failed! please try later...\n");
							int sum = strlen(buf);
							while (sum > 0)
							{
								int n = send(cfd, buf, strlen(buf), 0);
								sum -= n;
							}
						}
					}
				}
				else if (strncasecmp("drop", buffer, 4) == 0) // drop/202208010103/AR03024
				{

					string s = buffer;
					string stu_id = s.substr(5, 12);
					string cou_id = s.substr(18, strlen(buffer) - 19); //获得学生id 和课程id
																	   //	cout << stu_id << " " << cou_id << endl;
					string mess = search_one_stu(rf, stu_id);		   //查看是否有错误
					string mess2 = search_one_course(rf, cou_id);
					if ("error" == mess.substr(0, 5) || "error" == mess2.substr(0, 5)) //命令不对
					{
						mess = "stu_id or course error";
						// cout << "stu_id or course error" << endl;
						sprintf(buf, "%s", mess.c_str());
						int sum = strlen(buf);
						while (sum > 0)
						{
							int n = send(cfd, buf, strlen(buf), 0);
							sum -= n;
						}
					}
					else //命令ok
					{
						cout << "id ok\n";
						// stu_id  cou_id都没问题
						// entry index term command commit_index  //加入entry  命令 票数 等待

						int index = rf->len; // index是从1算起
						int term = rf->term;
						string com = s.substr(0, strlen(buffer) - 1); //去除\n
						rf->log_list[rf->len].index = index;		  //加入新的条目
						rf->log_list[rf->len].term = term;
						rf->log_list[rf->len].commend = com;
						int count = 1; //自己一票
						for (int i = 0; i < node_num; i++)
						{
							char it = 'A';
							it = char(it + i);
							if (it == rf->id)
								continue;
							memset(buf, 0, strlen(buf));
							sprintf(buf, "%s", "is_connect?\n");
							int x = create_TCP(buf, rf, it); //发送给除了自己的每个人
							if (x == 0)						 //如果消息正常发送 就暂且认为ok了
								count++;
						}

						if (count > floor(node_num / 2))
						{
							//执行提交任务 更新自己参数噢
							rf->len++;			//任务成功才保留这个条目
							rf->commit_index++; //应该提交的index  1
							cout << "我的commit_index=" << rf->commit_index << ". 给follower发送entry\n";
							for (int i = 0; i < node_num; i++) //发送给除了自己的每个人
							{
								char it = 'A';
								it = char(it + i);
								if (it == rf->id)
									continue;
								send_entry(rf, it);
							}

							memset(buf, 0, strlen(buf)); //发送给客户端说自己 ok了
							sprintf(buf, "%s", "Done!");
							int sum = strlen(buf);
							while (sum > 0)
							{
								int n = send(cfd, buf, strlen(buf), 0);
								sum -= n;
							}
						}
						else
						{
							rf->log_list[rf->len].index = rf->log_list[rf->len].term = 0; //清空

							memset(buf, 0, strlen(buf));
							sprintf(buf, "%s", "busy! failed! please try later...\n");
							int sum = strlen(buf);
							while (sum > 0)
							{
								int n = send(cfd, buf, strlen(buf), 0);
								sum -= n;
							}
						}
					}
				}
				else
				{
					sprintf(buf, "%s", "格式错误,请重新输入");
					int sum = strlen(buf);
					while (sum > 0)
					{
						int n = send(cfd, buf, strlen(buf), 0);
						sum -= n;
					}
				}
			}
			else if (rf->leader_id != '0')
			{												//有leader
				sprintf(buf, "%d", id_port[rf->leader_id]); //发送leader port
															//	cout << "buf " << buf << endl;
				int sum = strlen(buf);
				while (sum > 0)
				{
					int n = send(cfd, buf, strlen(buf), 0);
					sum -= n;
				}
			}
			else
			{
				sprintf(buf, "%d", id_port[rf->id]); //没有leader 发送自己 port
				int sum = strlen(buf);
				while (sum > 0)
				{
					int n = send(cfd, buf, strlen(buf), 0);
					sum -= n;
				}
			}
		}
	}
}

string search_one_course(raft_node *rf, string cou_id)
{
	string path = "data";
	char id = rf->id;
	string filename = "/courses.txt";
	string file = path + id + filename;
	ifstream fin(file);
	int pos = -1;
	string s;
	while (getline(fin, s))
	{
		pos = s.find(cou_id);
		if (pos != -1)
		{
			break;
			//	cout << s << endl;
		}
		else
		{
			s.clear();
		}
	}
	fin.close();
	if (pos == -1 || cou_id.length() < 7)
		s = "error course_id! ";
	// cout << "Done!\n";
	return s;
}

int create_listen_fd(int port, int epfd, int ip)
{
	//创建监听的套接字
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
	{
		perror("socket error");
		exit(1);
	}

	//设置本地IP和port
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(ip);
	server.sin_port = htons(port);

	// socket 端口复用, 防止测试的时候出现 Address already in use
	int on = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (-1 == ret)
	{
		perror("Set socket");
		return 0;
	}

	//监听套接字socket绑定端口
	ret = bind(lfd, (struct sockaddr *)&server, sizeof(server));
	if (ret == -1)
	{
		perror("bind error");
		exit(1);
	}

	// 设置监听
	ret = listen(lfd, 64);
	if (ret == -1)
	{
		perror("listen error");
		exit(1);
	}

	// lfd添加到epoll树上
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev); // 设置epoll的事件
	if (ret == -1)
	{
		perror("epoll_ctl add lfd error");
		exit(1);
	}
	return lfd;
}

void my_accept(int listenfd, int epfd)
{ //建立TCP连接
	struct sockaddr client_addr;
	socklen_t len = sizeof(client_addr);
	int client_fd = accept(listenfd, &client_addr, &len);
	if (client_fd == -1)
	{
		perror("accept error");
		exit(1);
	}

	// 设置cfd为非阻塞
	int flag = fcntl(client_fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(client_fd, F_SETFL, flag);

	// 得到的新节点挂到epoll树上
	struct epoll_event ev;
	ev.data.fd = client_fd;
	ev.events = EPOLLIN | EPOLLET; // 边沿非阻塞模式
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
	if (ret == -1)
	{
		perror("epoll_ctl add cfd error");
		exit(1);
	}
}

// 断开连接的函数
void disconnect(int cfd, int epfd)
{
	// 从epoll的树上摘下来
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret == -1)
	{
		perror("epoll_ctl del cfd error");
		exit(1);
	}
	close(cfd);
}

bool my_select(raft_node *rf)
{
	// cout << rf->id << " start select\n";
	char buf[4096] = {0};		 //发送数据缓冲区
	rf->heart_time_s = clock();	 //更新心跳的时间 防止超时
	rf->select_time_s = clock(); //开始选举时间计时

	for (int i = 0; i < node_num; i++)
	{
		char it = 'A';
		it = char(it + i);
		if (it == rf->id) //发送给除了自己的其他结点
			continue;

		memset(buf, 0, strlen(buf));
		sprintf(buf, "%s %d %c\r\n", "select", rf->term, rf->id); //事件是select 请投票
		create_TCP(buf, rf, it);								 //rf发给id为it的结点
		 // cout << "send select to " << it << " over" << endl;
	}
}

void send_heart_beat(raft_node *rf) // leader rf广播心跳
{
	char buf[4096] = {0}; // 发送数据缓冲区
	rf->send_heart = clock();//更新心跳发送频率计时器
	for (int i = 0; i < node_num; i++)
	{
		char it = 'A';
		it = char(it + i);
		if (it == rf->id) //发送给除了自己的每个人
			continue;

		memset(buf, 0, strlen(buf));
		sprintf(buf, "%s %d %c\r\n", "heart_beat", rf->term, rf->id);
		create_TCP(buf, rf, it);
		cout << "send heat_beat to " << it << endl;
	}
}

void send_entry(raft_node *rf, char it) // leader 发送给it
{
	// cout << "输出所有条目\n";
	// for (int k = 0; k < rf->len; k++)
	//	printf("k=%d,rf->log_list[k].index=%d,rf->log_list[k].term=%d,rf->log_list[k].commend=%s\n", k, rf->log_list[k].index, rf->log_list[k].term, rf->log_list[k].commend.c_str());
	char buf[4096] = {0}; // 发送数据缓冲区
						  // entry leader_term index term command commit_index  prev_term,prev_index //加入entry  命令 票数 等待

	//发送 type,leader的term,结点应该接收到的条目index,term,com,leader应该提交的index,prev_term,prev_index
	int i = 0;
	if (it == 'B')
		i = 1;
	if (it == 'C')
		i = 2;
	int index = rf->log_list[rf->next_index[i]].index;
	int term = rf->log_list[rf->next_index[i]].term;
	string com = rf->log_list[rf->next_index[i]].commend;
	int prev_index = rf->log_list[rf->next_index[i] - 1].index; // 0
	int prev_term = rf->log_list[rf->next_index[i] - 1].term;	// 0
	string prev_com = rf->log_list[rf->next_index[i] - 1].commend;
	rf->next_index[i]++;
	rf->match_index[i]++;
	printf("send entry to follower: %c,\n l_team = %d, index = %d, term = %d, command = %s, commit_index = %d, prev_index = %d,prev_term = %d \n",
		   it, rf->term, index, term, com.c_str(), rf->commit_index, prev_index, prev_term);

	memset(buf, 0, strlen(buf));
	sprintf(buf, "%s %d %d %d %s %d %d %d %s", "entry", rf->term, index, term, com.c_str(), rf->commit_index, prev_index, prev_term, prev_com.c_str());
	create_TCP(buf, rf, it);
	// cout << "send entry to " << it << endl;
}

int create_TCP(char *buf, raft_node *rf, char it) //和it的结点建立连接   rf发给it
{
	int port = id_port[it];
	string temp = id_ip[it];
	const char *ip = temp.c_str(); // string 转 char*

	struct in_addr server_addr;		  //要访问的server地址
	if (!inet_aton(ip, &server_addr)) //将argc[1] IP地址->网络字节序 存储在server_addr中
		perror("inet_aton");
	struct sockaddr_in remote_addr;
	memset(&remote_addr, 0, sizeof(remote_addr)); //初始化清空
	remote_addr.sin_family = AF_INET;			  ////指定IP地址地址版本为IPV4
	remote_addr.sin_addr = server_addr;			  //保存server的IP地址信息(网络字节序)
	remote_addr.sin_port = htons(port);			  //保存端口号
	int *client_sockfd = new int;				  //显示分配内存int
	if ((*client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{ //给socket描述符 创建socket
		perror("socket");
		return 1;
	}
	// cout << "socket ok\n";

	if (connect(*client_sockfd, (const struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
	{ // socket尝试与服务器建立连接
		perror("connect");
		return 1;
	}
	struct sockaddr_in localAddress;
	socklen_t addressLength = sizeof(localAddress);
	getsockname(*client_sockfd, (struct sockaddr *)&localAddress, &addressLength); //获得本地client 的IP和socket的端口号

	// printf("I'm %s:%d \t connected to server %s:%d\n", inet_ntoa(localAddress.sin_addr), ntohs(localAddress.sin_port) //输出 client的 IP和端口
	//	   ,
	//	   inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port)); //输出 server的 IP和端口

	my_send(client_sockfd, buf); //发送的动作
	return 0;					 //正常发送
}

void *my_send(void *args, char *buf) //发送buf的内容给socket
{
	int *sockfdAddr = (int *)args;
	int sockfd = *sockfdAddr;

	int n;
	// cout << " try to send " << buf << endl;

	int num = strlen(buf); //发送
	while (num > 0)		   // if sndbuf is null break
	{
		n = write(sockfd, buf, strlen(buf)); // send sndbuf to server
		if (n <= 0)
		{ // send fail
			perror("Error: write to socket");
			close(sockfd);
			free(sockfdAddr);
			return NULL;
		}
		num -= n;
	}

	close(sockfd); // close socket
	free(sockfdAddr);
	return NULL;
}

int ip_int(string temp_ip)
{

	const char *ip = temp_ip.c_str();
	unsigned int re = 0;
	unsigned char tmp = 0;
	while (1)
	{
		if (*ip != '\0' && *ip != '.')
		{
			tmp = tmp * 10 + *ip - '0';
		}
		else
		{
			re = (re << 8) + tmp;
			if (*ip == '\0')
				break;
			tmp = 0;
		}
		ip++;
	}
	return re;
}

string search_one_stu(raft_node *rf, string cou_id) // 12位
{
	string path = "data";
	char id = rf->id;
	string filename = "/students.txt";
	string file = path + id + filename;
	ifstream fin(file);
	int pos = -1;
	string s;
	while (getline(fin, s))
	{
		pos = s.find(cou_id);
		if (pos != -1)
		{
			break;
			//	cout << s << endl;
		}
		else
		{
			s.clear();
		}
	}
	fin.close();
	if (pos == -1 || cou_id.length() != 12)
	{
		s = "error student_id!";
	}
	return s;
}

// 16进制数转化为10进制
int hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

void decode_str(char *to, char *from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			*to = hexit(from[1]) * 16 + hexit(from[2]);
			from += 2;
		}
		else
		{
			*to = *from;
		}
	}
	*to = '\0';
}

void commit_task(raft_node *rf)
{
	string commend;
	commend = rf->log_list[rf->last_applied].commend;
	cout << commend << endl; //命令
	char type[10], stu_id[15], cou_id[15];
	sscanf(commend.c_str(), "%[^/]/%[^/]/%[^/]", type, stu_id, cou_id);
	cout << type << " " << stu_id << " " << cou_id << endl;
	// if (strncasecmp("drop", type, 4) == 0)
	// {
	// 	string path = "data";
	// 	char id = rf->id;
	// 	string filename = "/students.txt";
	// 	string file = path + id + filename;
	// 	ifstream fin(file);
	// 	string s;
	// 	while (getline(fin, s)) //
	// 	{
	// 		char temp[100];
	// 		strcpy(temp, s.c_str());
	// 		decode_str(temp, temp); //中文乱码问题
	// 		int pos = s.find(cou_id);
	// 		if (pos != -1)//s=那一行
	// 		{
	// 			break;
	// 		}
	// 		s.clear();
	// 	}
	// 	fin.close();
	// }
	// else
	// {
	// 	string path = "data";
	// 	char id = rf->id;
	// 	string filename = "/students.txt";
	// 	string file = path + id + filename;
	// 	ifstream fin(file);
	// 	string s;
	// 	while (getline(fin, s)) //太多信息 只能一部份一部分发
	// 	{
	// 		char temp[100];
	// 		strcpy(temp, s.c_str());
	// 		decode_str(temp, temp); //中文乱码问题
	// 		int pos = s.find(cou_id);
	// 		if (pos != -1)//s=那一行
	// 		{
	// 			break;
	// 		}
	// 		s.clear();
	// 	}
	// 	fin.close();
	// }
}