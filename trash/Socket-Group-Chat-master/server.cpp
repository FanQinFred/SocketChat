/*
 *��������ⲩ�͵�ַ��http://blog.csdn.net/qq_18297675/article/details/52819975
 *������д��2016.10.14 12��00 -- 21��50

 *�������ֻ��̽�ֶ����������ĳ��� -- 1V1���죬��ȴ���Ǽ򵥵�C-S�Ի���
 *����C-S-C������clientͨ��Sserverת����Ϣ��
 
 *����BUG���£�
 *���ر�һ�����ӵ�ʱ��CPU��ͻȻ���ǣ�����ԭ���Ҳ����˺ö����Ҳû�ҳ��������
 *�д����ҳ����ˣ�ϣ���ܵ�����������һ�£���л~

 *����bug����Ϊû��ʱ��ר�Ų��ԣ�������δ֪����
*/


#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib,"ws2_32.lib")

#define SEND_OVER 1							 //�Ѿ�ת����Ϣ
#define SEND_YET  0							 //��ûת����Ϣ

int g_iStatus = SEND_YET;
SOCKET g_ServerSocket = INVALID_SOCKET;		 //������׽���
SOCKADDR_IN g_ClientAddr = { 0 };			 //�ͻ��˵�ַ
int g_iClientAddrLen = sizeof(g_ClientAddr);
bool g_bCheckConnect = false;                //����������
HANDLE g_hRecv1 = NULL;
HANDLE g_hRecv2 = NULL;
//�ͻ�����Ϣ�ṹ��
typedef struct _Client
{
	SOCKET sClient;      //�ͻ����׽���
	char buf[128];		 //���ݻ�����
	char userName[16];   //�ͻ����û���
	char IP[20];		 //�ͻ���IP
	UINT_PTR flag;       //��ǿͻ��ˣ��������ֲ�ͬ�Ŀͻ���
}Client;

Client g_Client[2] = { 0 };                  //����һ���ͻ��˽ṹ��

//���������߳�
unsigned __stdcall ThreadSend(void* param)
{
	int ret = 0;
	int flag = *(int*)param;
	SOCKET client = INVALID_SOCKET;					//����һ����ʱ�׽��������Ҫת���Ŀͻ����׽���
	char temp[128] = { 0 };							//����һ����ʱ�����ݻ�������������Ž��յ�������
	memcpy(temp, g_Client[!flag].buf, sizeof(temp));
	sprintf(g_Client[flag].buf, "%s: %s", g_Client[!flag].userName, temp);//���һ���û���ͷ

	if (strlen(temp) != 0 && g_iStatus == SEND_YET) //������ݲ�Ϊ���һ�ûת����ת��
		ret = send(g_Client[flag].sClient, g_Client[flag].buf, sizeof(g_Client[flag].buf), 0);
	if (ret == SOCKET_ERROR)
		return 1;
	g_iStatus = SEND_OVER;   //ת���ɹ�������״̬Ϊ��ת��
	return 0;
}

//��������
unsigned __stdcall ThreadRecv(void* param)
{
	SOCKET client = INVALID_SOCKET;
	int flag = 0; 
	if (*(int*)param == g_Client[0].flag)            //�ж����ĸ��ͻ��˷�������Ϣ
	{
		client = g_Client[0].sClient;
		flag = 0;
	}	
	else if (*(int*)param == g_Client[1].flag)
	{
		client = g_Client[1].sClient;
		flag = 1;
	}
	char temp[128] = { 0 };  //��ʱ���ݻ�����
	while (1)
	{
		memset(temp, 0, sizeof(temp));
		int ret = recv(client, temp, sizeof(temp), 0); //��������
		if (ret == SOCKET_ERROR)
			continue;
		g_iStatus = SEND_YET;								 //����ת��״̬Ϊδת��
		flag = client == g_Client[0].sClient ? 1 : 0;        //���Ҫ���ã����������Լ����Լ�����Ϣ��BUG
		memcpy(g_Client[!flag].buf, temp, sizeof(g_Client[!flag].buf));
		_beginthreadex(NULL, 0, ThreadSend, &flag, 0, NULL); //����һ��ת���߳�,flag�����Ҫת�����ĸ��ͻ���
		//����Ҳ�����ǵ���CPUʹ����������ԭ��
	}
		
	return 0;
}

//��������
unsigned __stdcall ThreadManager(void* param)
{
	while (1)
	{
		if (send(g_Client[0].sClient, "", sizeof(""), 0) == SOCKET_ERROR)
		{
			if (g_Client[0].sClient != 0)
			{
				CloseHandle(g_hRecv1); //����ر����߳̾�������ǲ��Խ���Ͽ���C/S�Ӻ�CPU��Ȼ����
				CloseHandle(g_hRecv2);
				printf("Disconnect from IP: %s,UserName: %s\n", g_Client[0].IP, g_Client[0].userName);
				closesocket(g_Client[0].sClient);   //����򵥵��жϣ���������Ϣʧ�ܣ�����Ϊ�����ж�(��ԭ���ж���)���رո��׽���
				g_Client[0] = { 0 };
			}
		}
		if (send(g_Client[1].sClient, "", sizeof(""), 0) == SOCKET_ERROR)
		{
			if (g_Client[1].sClient != 0)
			{
				CloseHandle(g_hRecv1);
				CloseHandle(g_hRecv2);
				printf("Disconnect from IP: %s,UserName: %s\n", g_Client[1].IP, g_Client[1].userName);
				closesocket(g_Client[1].sClient);
				g_Client[1] = { 0 };
			}
		}
		Sleep(2000); //2s���һ��
	}

	return 0;
}

//��������
unsigned __stdcall ThreadAccept(void* param)
{

	int i = 0;
	int temp1 = 0, temp2 = 0;
	_beginthreadex(NULL, 0, ThreadManager, NULL, 0, NULL);
	while (1)
	{
		while (i < 2)
		{
			if (g_Client[i].flag != 0)
			{
				++i;
				continue;
			}
			//����пͻ����������Ӿͽ�������
			if ((g_Client[i].sClient = accept(g_ServerSocket, (SOCKADDR*)&g_ClientAddr, &g_iClientAddrLen)) == INVALID_SOCKET)
			{
				printf("accept failed with error code: %d\n", WSAGetLastError());
				closesocket(g_ServerSocket);
				WSACleanup();
				return -1;
			}
			recv(g_Client[i].sClient, g_Client[i].userName, sizeof(g_Client[i].userName), 0); //�����û���
			printf("Successfuuly got a connection from IP:%s ,Port: %d,UerName: %s\n",
				inet_ntoa(g_ClientAddr.sin_addr), htons(g_ClientAddr.sin_port), g_Client[i].userName);
			memcpy(g_Client[i].IP, inet_ntoa(g_ClientAddr.sin_addr), sizeof(g_Client[i].IP)); //��¼�ͻ���IP
			g_Client[i].flag = g_Client[i].sClient; //��ͬ��socke�в�ͬUINT_PTR���͵���������ʶ
			i++;
		}
		i = 0;
		
		if (g_Client[0].flag != 0 && g_Client[1].flag != 0)					 //�������û��������Ϸ�������Ž�����Ϣת��
		{
			if (g_Client[0].flag != temp1)     //ÿ�ζϿ�һ�����Ӻ��ٴ����ϻ��¿�һ���̣߳�����cpuʹ��������,����Ҫ�ص��ɵ�
			{
				if (g_hRecv1)                  //����ر����߳̾�������ǲ��Խ���Ͽ���C/S�Ӻ�CPU��Ȼ����
 					CloseHandle(g_hRecv1);
				g_hRecv1 = (HANDLE)_beginthreadex(NULL, 0, ThreadRecv, &g_Client[0].flag, 0, NULL); //����2��������Ϣ���߳�
			}	
			if (g_Client[1].flag != temp2)
			{
				if (g_hRecv2)
					CloseHandle(g_hRecv2);
				g_hRecv2 = (HANDLE)_beginthreadex(NULL, 0, ThreadRecv, &g_Client[1].flag, 0, NULL);
			}		
		}
		
		temp1 = g_Client[0].flag; //��ֹThreadRecv�̶߳�ο���
		temp2 = g_Client[1].flag;
			
		Sleep(2000);
	}

	return 0;
}

//����������
int StartServer()
{
	//����׽�����Ϣ�Ľṹ
	WSADATA wsaData = { 0 };
	SOCKADDR_IN ServerAddr = { 0 };				//����˵�ַ
	USHORT uPort = 18000;						//�����������˿�

	//��ʼ���׽���
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("WSAStartup failed with error code: %d\n", WSAGetLastError());
		return -1;
	}
	//�жϰ汾
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("wVersion was not 2.2\n");
		return -1;
	}
	//�����׽���
	g_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_ServerSocket == INVALID_SOCKET)
	{
		printf("socket failed with error code: %d\n", WSAGetLastError());
		return -1;
	}

	//���÷�������ַ
	ServerAddr.sin_family = AF_INET;//���ӷ�ʽ
	ServerAddr.sin_port = htons(uPort);//�����������˿�
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//�κοͻ��˶����������������

	//�󶨷�����
	if (SOCKET_ERROR == bind(g_ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		printf("bind failed with error code: %d\n", WSAGetLastError());
		closesocket(g_ServerSocket);
		return -1;
	}
	//���ü����ͻ���������
	if (SOCKET_ERROR == listen(g_ServerSocket, 20000))
	{
		printf("listen failed with error code: %d\n", WSAGetLastError());
		closesocket(g_ServerSocket);
		WSACleanup();
		return -1;
	}

	_beginthreadex(NULL, 0, ThreadAccept, NULL, 0, 0);
	for (int k = 0;k < 100;k++) //�����߳����ߣ��������ر�TCP����.
		Sleep(10000000);
	
	//�ر��׽���
	for (int j = 0;j < 2;j++)
	{
		if (g_Client[j].sClient != INVALID_SOCKET)
			closesocket(g_Client[j].sClient);
	}
	closesocket(g_ServerSocket);
	WSACleanup();
	return 0;
}

int main()
{
	StartServer(); //����������
	
	return 0;
}