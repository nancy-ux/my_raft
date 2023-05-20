#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include<string.h>

#define MAXIN 999
#define MAXOUT 999
#define MAX_THREAD_NUM 8092
using namespace std;

bool sign;
char rcvbuf[MAXIN];
char sndbuf[MAXIN];

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

char *getreq(char *inbuf, int len)
{
    memset(inbuf, 0, len);           // clear inbuf
    return fgets(inbuf, len, stdin); // get char stream to inbuf
}

int client(void *args, int port)
{
    int n;
    int *sockfdAddr = (int *)args;
    int sockfd = *sockfdAddr;
    cout << ">";
    getreq(sndbuf, MAXIN);  //键盘输入
    if (strncasecmp("exit", sndbuf, 4) != 0) //输入exit 说明客户端想退出
    {
        n = write(sockfd, sndbuf, strlen(sndbuf)); // 发送数据
        if (n <= 0)
        { // send fail
            perror("Error: write to socket");
            close(sockfd);
            free(sockfdAddr);
            return -1;
        }
        memset(rcvbuf, 0, MAXOUT); // clear rcvbuf



        n = read(sockfd, rcvbuf, MAXOUT - 1); // get rcvbuf from server  接收数据
        if (n <= 0){ // recive fail
            perror("Error: read from socket");//客户端没有接收到数据
            close(sockfd);
            free(sockfdAddr);
            return -1;
        }
        while (n > 0)//接收到数据
        {
            decode_str(rcvbuf,rcvbuf);
            cout << rcvbuf ;//输出
            if (strncasecmp("8000", rcvbuf, 4) == 0 || strncasecmp("8001", rcvbuf, 4) == 0 || strncasecmp("8002", rcvbuf, 4) == 0)
            {
                //结束此次连接
                cout << "\n访问的不是leader or 集群暂时没有 leader,重新访问连接中...\n";
                close(sockfd);       // close socket
                return atoi(rcvbuf); //返回leader port
            }

            memset(rcvbuf, 0, MAXOUT);//清空
            n = read(sockfd, rcvbuf, MAXOUT - 1); // get rcvbuf from server  接收数据
        }

        cout<<endl;
        close(sockfd); //结束连接
        return port;   //返回可以继续连接的Port
    }
    else
    { //结束所有连接
        close(sockfd);
        free(sockfdAddr);
        return -1;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    { //输入参数: 访问服务端的ip port
        cout << "IP is default=0.0.0.0 \n Please print : port \n";
        exit(1);
    }

    const char* ip="0.0.0.0";
    //实例化
    struct in_addr server_addr;
    if (!inet_aton(ip, &server_addr)) //将argc[1] IP地址->网络字节序 存储在server_addr中
        perror("inet_aton");
    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr)); //初始化清空
    remote_addr.sin_family = AF_INET;             ////指定IP地址地址版本为IPV4
    remote_addr.sin_addr = server_addr;           //保存server的IP地址信息(网络字节序)
    int port = atoi(argv[1]);                     //端口号
    int *client_sockfd = new int;                 //显示分配内存int

    printf("请输入参数，格式为:操作码/内容\n");
    printf("------------------\n");
    printf("操作码      内容             解释                   eg:\n");
    printf("search_cou/cou_id        查询该课程信息             search_cou/AR03024\n");
    // printf("search_cou/index=x       查询从x行起9个课程的信息    search_cou/index=3\n");
    printf("search_cou/all           查询所有课程信息          search_cou/all\n");
    printf("search_stu/stu_id        查询学生信息                search_stu/202208010102\n");
    printf("choose/stu_id/cou_id     学生选课                   choose/202208010103/AR03024\n");
    printf("drop/stu_id/cou_id       学生退课                   drop/202208010103/AR03024\n");
    printf("-------------------\n");

    while (port != -1)
    {
        if ((*client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) //给socket描述符 可以认为是创建socket
        {
            perror("socket");
            return 1;
        }
        struct sockaddr_in localAddress;
        socklen_t addressLength = sizeof(localAddress);
        remote_addr.sin_port = htons(port);                                                          //保存端口号
        if (connect(*client_sockfd, (const struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) // socket尝试与服务器建立连接
        {
            perror("connect");
            return 1;
        }
        getsockname(*client_sockfd, (struct sockaddr *)&localAddress, &addressLength); //获得本地client 的IP和socket的端口号
        port = client(client_sockfd, port);
    }
    return 0;
}
