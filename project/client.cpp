#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <sys/errno.h>
#include <sys/select.h>
#include "header/Message/Message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATA_SIZE 1518

#define UDP_DATA_MAXSIZE 65527
#define IPV4_DATA_MAXSIZE 1440
#define DATALINK_DATA_MAXSIZE 1500

///////////////////////工具-START////////////////////////////////
// OpenFile more safe
FILE *OpenFile(char *fileinput)
{
    FILE *file;
    if ((file = fopen(fileinput, "r+")) == NULL)
    {
        printf("%s\n", "File Open Error!");
        exit(0);
    }
    return file;
}

// Tell file frame position
long int ftellSafe(FILE *file)
{
    long int pos;
    if ((pos = ftell(file)) == -1L)
    {
        exit(0);
    }
    return pos;
}
///////////////////////工具-END////////////////////////////////

///////////////////////数据链路层-发送-START////////////////////////////////
#define MAXSIZE 1500
#define MINSIZE 46

// SIZE:    6Bytes   6Bytes   2Bytes         46-1500Bytes   4Bytes
// MEANING: DA       SA       ProtocalType   PayLoad        FCS

// Mac Address
typedef unsigned char mac_addr[6];
mac_addr DesMacAddr = {0x3D, 0xE1, 0x2D, 0x6F, 0xE9, 0x34};
mac_addr SrcMacAddr = {0x34, 0xE1, 0x2D, 0x6F, 0xE9, 0x3D};

// Data source and data destination
FILE *fileIn, *fileOut;
// The position of the last frame
long int LastFramePos;
long int PayLoadCount, AllByteCount;
long int RestByteCount, LackByteCount;

// CRC checker
unsigned int crc32(unsigned char *data, int len)
{
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
    {
        crc = crc ^ data[i];
        for (int j = 0; j < 8; j++)
        {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

// Merge data to form frames
unsigned short make_frame(mac_addr *dst, mac_addr *src, unsigned short protocol, unsigned char *payload, int payloadlen, unsigned char *result)
{
    memcpy(&result[0], dst, 6);
    memcpy(&result[6], src, 6);
    memcpy(&result[12], &protocol, sizeof(protocol));
    memcpy(&result[14], payload, payloadlen);
    unsigned int crc32_result = crc32(result, payloadlen + 14);
    memcpy(&result[14 + payloadlen], &crc32_result, sizeof(crc32_result));
    return 18 + payloadlen;
}

// Send a frame
void send_frame(unsigned char *frame_data, unsigned short len, int server_sock, char *address)
{
    printf("send_frame-len: %d\n", len);
    Message send_message;
    for (int kk = 0; kk < (len + 6); kk++)
    {
        printf("%c", frame_data[kk]);
    }
    printf("\n");

    send_message.setMessageWithLen((char *)frame_data, (unsigned int)len, (char *)address, (unsigned int)(strlen(address)));
    
    ssize_t send_len = send(server_sock, (char *)&send_message, sizeof(Message), 0);
    if (send_len <= 0)
    {
        perror("write failed");
        exit(1);
    }

    //   fwrite(&len, sizeof(len), 1, file);   //发送帧的长度
    //   fwrite(frame_data, sizeof(char), len, file);  //发送帧
}

// Start send
void datalink_layer_send(unsigned char *buf, int len, int server_sock, char *address)
{
    unsigned char FrameBuffer[DATALINK_DATA_MAXSIZE + 18];
    unsigned short FrameLength = make_frame(&DesMacAddr, &SrcMacAddr, 0x0800, buf, len, FrameBuffer);
    printf("FrameLength: %d\n", FrameLength);
    send_frame(FrameBuffer, FrameLength, server_sock, address);
}
///////////////////////数据链路层-发送-END////////////////////////////////

///////////////////////网络层-发送-START////////////////////////////////
#define DATA_MAXSIZE 1440 //字节
#define DATA_MINSIZE 26   //字节

struct IP_Packet
{
    // 4 bit
    unsigned int IPv4_Version : 4;
    // 4 bit
    unsigned int IPv4_IHL : 4;
    // 8 bit
    unsigned int IPv4_TOS : 8;

    // 16 bit
    unsigned int IPv4_TotalLength : 16;

    // 16 bit
    unsigned int IPv4_Identification : 16;

    // 1 bit
    unsigned int IPv4_NoFunc : 1;
    // 1 bit
    unsigned int IPv4_DF : 1;
    // 1 bit
    unsigned int IPv4_MF : 1;
    // 13 bit
    unsigned int IPv4_FragmentOffset : 13;

    // 8 bit
    unsigned int IPv4_TimeTolive : 8;
    // 8 bit
    unsigned int IPv4_Protocol : 8;

    // 16 bit
    unsigned int IPv4_HeaderCheckSum : 16;

    // 32 bit
    unsigned int IPv4_SourceAddr : 32;

    // 32 bit
    unsigned int IPv4_DesAddr : 32;

    // 40 Byte
    unsigned char IPv4_Option[40];

    // 26~1480 Byte
    unsigned char IPv4_Data[DATA_MAXSIZE];
};

void PrintBinary(const int argc)
{
    for (int i = 15; i >= 0; i--) //高位到低位输出
    {
        int a;
        a = 0x01 & argc >> i;
        printf("%d", a);
    }
}

unsigned short extendl_16bit(unsigned int bit_content, int bit_offset)
{
    bit_content = bit_content << bit_offset;
    unsigned short res = 0b0; //16 bit
    res += bit_content;
    return res;
}

unsigned short extendr_16bit(unsigned int bit_content, int bit_offset)
{
    bit_content = bit_content >> bit_offset;
    unsigned short res = 0b0; //16 bit
    res += bit_content;
    return res;
}

unsigned char extendl_8bit(unsigned int bit_content, int bit_offset)
{
    bit_content = bit_content << bit_offset;
    unsigned char res = 0b0; //16 bit
    res += bit_content;
    return res;
}

unsigned char extendr_8bit(unsigned int bit_content, int bit_offset)
{
    bit_content = bit_content >> bit_offset;
    unsigned char res = 0b0; //16 bit
    res += bit_content;
    return res;
}

// CheckSum
void HeaderSetCheckSum(IP_Packet &ip_packet)
{
    ip_packet.IPv4_HeaderCheckSum =

        extendl_16bit(ip_packet.IPv4_Version, 12) +
        extendl_16bit(ip_packet.IPv4_IHL, 8) +
        extendl_16bit(ip_packet.IPv4_TOS, 0) +

        extendl_16bit(ip_packet.IPv4_TotalLength, 0) +

        extendl_16bit(ip_packet.IPv4_Identification, 0) +

        extendl_16bit(ip_packet.IPv4_NoFunc, 15) +
        extendl_16bit(ip_packet.IPv4_DF, 14) +
        extendl_16bit(ip_packet.IPv4_MF, 13) +
        extendl_16bit(ip_packet.IPv4_FragmentOffset, 0) +

        extendl_16bit(ip_packet.IPv4_TimeTolive, 8) +
        extendl_16bit(ip_packet.IPv4_Protocol, 0) +

        extendl_16bit(ip_packet.IPv4_SourceAddr, 0) + //低16bit
        extendr_16bit(ip_packet.IPv4_SourceAddr, 16) +

        extendl_16bit(ip_packet.IPv4_DesAddr, 0) + //低16bit
        extendr_16bit(ip_packet.IPv4_DesAddr, 16);
}

//unsigned long IpPacketLen=MakeIpPacket(ip_packet_info,ipv4_buffer,ip_packet_info.IPv4_Option,40,udp_packet,udp_packet_len);

// Make Ip packet
unsigned int MakeIpPacket(unsigned int DF, unsigned int MF, unsigned int FragmentOffset, const IP_Packet ip_packet, unsigned char *buf, unsigned char *IPv4_Option, long IPv4_Option_Len, unsigned char *IPv4_Data, short IPv4_Data_Len)
{
    //第一个byte
    unsigned char VersionAndIhl = extendl_8bit(ip_packet.IPv4_Version, 4) + extendl_8bit(ip_packet.IPv4_IHL, 0); //8 bit
    memcpy(buf, &VersionAndIhl, sizeof(VersionAndIhl));
    //第二个byte
    unsigned char IPv4_TOS = extendl_8bit(ip_packet.IPv4_TOS, 0); //8 bit
    memcpy(&buf[1], &IPv4_TOS, sizeof(IPv4_TOS));                 //1已经被占用
    //第三四个byte
    unsigned short IPv4_TotalLength = 5 + 40 + (short)IPv4_Data_Len; //ip_packet.IPv4_TotalLength;
    memcpy(&buf[2], &IPv4_TotalLength, sizeof(IPv4_TotalLength));    //3已经被占用
    //第五六个byte
    unsigned short IPv4_Identification = ip_packet.IPv4_Identification;
    memcpy(&buf[4], &IPv4_Identification, sizeof(IPv4_Identification)); //5已经被占用
    //第7、8个byte
    //unsigned short extendl_16bit(unsigned int bit_content, int bit_offset)
    unsigned short NoFunc_DF_FragmentOffset = extendl_16bit(ip_packet.IPv4_NoFunc, 15) + extendl_16bit(DF, 14) + extendl_16bit(MF, 13) + extendl_16bit(FragmentOffset, 0);
    memcpy(&buf[6], &NoFunc_DF_FragmentOffset, sizeof(NoFunc_DF_FragmentOffset)); //7已经被占用
    //第9、10个byte
    //unsigned short TimeTolive_Protocol=extendl_16bit(ip_packet.IPv4_TimeTolive,8)+extendl_16bit(ip_packet.IPv4_Protocol,0);
    unsigned short TimeTolive_Protocol = extendl_16bit(ip_packet.IPv4_Protocol, 8) + extendl_16bit(ip_packet.IPv4_TimeTolive, 0);
    memcpy(&buf[8], &TimeTolive_Protocol, sizeof(TimeTolive_Protocol)); //9已经被占用
    //11 12
    unsigned short HeaderCheckSum = ip_packet.IPv4_HeaderCheckSum;
    memcpy(&buf[10], &HeaderCheckSum, sizeof(HeaderCheckSum)); //11已经被占用
    //13 14 15 16
    unsigned int SourceAddr = ip_packet.IPv4_SourceAddr;
    memcpy(&buf[12], &SourceAddr, sizeof(SourceAddr)); //15已经被占用
    //17 18 19 20
    unsigned int DesAddr = ip_packet.IPv4_DesAddr;
    memcpy(&buf[16], &DesAddr, sizeof(DesAddr)); //19已经被占用
    // option
    memcpy(&buf[20], IPv4_Option, IPv4_Option_Len);
    // data
    memcpy(&buf[20 + IPv4_Option_Len], IPv4_Data, IPv4_Data_Len);
    //返回字节数
    return 20 + IPv4_Option_Len + IPv4_Data_Len;
}
//datalink_layer_send(buf, IpPacketLen,fileOut);
//将形成的UDP数据包发送给网络层处理
// network_layer_send(udp_buffer,UdpPacketLen,fileOut);
void network_layer_send(unsigned char *udp_packet, unsigned int udp_packet_len, int server_sock, char *address)
{
    printf("udp_packet_len: %d\n", udp_packet_len);
    struct IP_Packet ip_packet_info = {0b0100, 0b1111, 0b00000000,         //IPv4_Version,IPv4_IHL,IPv4_TOS
                                       0b0000000000000000,                 //IPv4_TotalLength
                                       0b0000000000000000,                 //IPv4_Identification
                                       0b0, 0b0, 0b0, 0b0000000000000,     //IPv4_NoFunc,IPv4_DF,IPv4_MF,IPv4_FragmentOffset
                                       0b01010101, 0b10101010,             //IPv4_TimeTolive,IPv4_Protocol
                                       0b0000000000000000,                 //IPv4_HeaderCheckSum
                                       0b11011010010001101111111101100001, //IPv4_SourceAddr //218.70.255.97
                                       0b01110010001101110101111100001110, //IPv4_DesAddr //114.55.95.14
                                       0b0,                                //IPv4_Option
                                       0b0};                               //IPv4_Data
    //开始分片
    // Split the data
    for (unsigned int j = 0; j <= udp_packet_len / 1440; j++)
    {
        // // 1 bit
        // unsigned int IPv4_NoFunc : 1;
        // // 1 bit
        // unsigned int IPv4_DF : 1; //DF=0允许分片；DF=1不允许分片。
        // // 1 bit
        // unsigned int IPv4_MF : 1;  //MF=1表示后面还有分片；MF=0表示这是最后一个分片
        // // 13 bit
        // unsigned int IPv4_FragmentOffset : 13;  //指明了每个分片相对于原始报文开头的偏移量，以8B为单位，即每个分片的长度必须是8B的整数倍。

        if (j != udp_packet_len / 1440)
        {
            // 0~IPV4_DATA_MAXSIZE-1 ; IPV4_DATA_MAXSIZE~2*IPV4_DATA_MAXSIZE-1;
            // j*IPV4_DATA_MAXSIZE~((j+1)*IPV4_DATA_MAXSIZE-1)
            unsigned char udp_packet_splited[IPV4_DATA_MAXSIZE];
            for (int i = j * IPV4_DATA_MAXSIZE, l = 0; i < ((j + 1) * IPV4_DATA_MAXSIZE - 1); i++, l++)
            {
                //printf("i:%d\n" ,i);
                udp_packet_splited[l] = udp_packet[i];
            }
            unsigned char ipv4_buffer[IPV4_DATA_MAXSIZE + 60]; //存放udp数据包
            unsigned int DF, MF, FragmentOffset;
            DF = 0;
            MF = 1;
            FragmentOffset = j;
            //unsigned int MakeIpPacket(unsigned int DF,unsigned int MF,unsigned int FragmentOffset, const IP_Packet ip_packet, unsigned char *buf, unsigned char *IPv4_Option, long IPv4_Option_Len, unsigned char *IPv4_Data, short IPv4_Data_Len)
            unsigned int IpPacketLen = MakeIpPacket(DF, MF, FragmentOffset, ip_packet_info, ipv4_buffer, ip_packet_info.IPv4_Option, 40, udp_packet_splited, 1440);
            datalink_layer_send(ipv4_buffer, IpPacketLen, server_sock, address);
        }
        else
        {
            int RestByte = udp_packet_len - (udp_packet_len / 1440) * IPV4_DATA_MAXSIZE;
            unsigned char udp_packet_splited[IPV4_DATA_MAXSIZE];
            for (int i = j * IPV4_DATA_MAXSIZE, l = 0; i < udp_packet_len; i++, l++)
            {
                udp_packet_splited[l] = udp_packet[i];
            }
            //以8B为单位，即每个分片的长度必须是8B的整数倍。
            if (RestByte / 8 != 0)
            {
                int ToFill = 8 - (RestByte - (RestByte / 8) * 8);
                for (int k = 0; k < ToFill; k++)
                {
                    udp_packet_splited[RestByte + k] = 0b0;
                }
                RestByte += ToFill;
            }
            unsigned char ipv4_buffer[IPV4_DATA_MAXSIZE + 60]; //存放udp数据包
            int DF, MF, FragmentOffset;
            DF = 0;
            MF = 0;
            FragmentOffset = j;
            unsigned long IpPacketLen = MakeIpPacket(DF, MF, FragmentOffset, ip_packet_info, ipv4_buffer, ip_packet_info.IPv4_Option, 40, udp_packet_splited, RestByte);
            datalink_layer_send(ipv4_buffer, IpPacketLen, server_sock, address);
        }
    }
}
///////////////////////网络层-发送-END////////////////////////////////

///////////////////////传输层-发送-START////////////////////////////////
struct UDP_Packet
{
    // 16 bit
    unsigned short UDP_SRC_PORT;
    // 16 bit
    unsigned short UDP_DES_PORT;
    // 16 Byte
    unsigned short UDP_LEN;
    // 16 Byte
    unsigned short UDP_CHECK_SUM;
    // (26-8)~(1480-8) Byte
    unsigned char UDP_Data[1472];
};

// Make Udp packet
unsigned long MakeUdpPacket(
    const UDP_Packet udp_packet,
    unsigned char *buf,
    unsigned int UDP_Data_Len,
    unsigned char *UDP_Data)
{
    unsigned short UDP_SRC_PORT = udp_packet.UDP_SRC_PORT;
    memcpy(buf, &UDP_SRC_PORT, sizeof(UDP_SRC_PORT));
    unsigned short UDP_DES_PORT = udp_packet.UDP_DES_PORT;
    memcpy(&buf[2], &UDP_DES_PORT, sizeof(UDP_DES_PORT));
    unsigned short UDP_LEN = UDP_Data_Len + 8; //udp_packet.UDP_LEN;
    memcpy(&buf[4], &UDP_LEN, sizeof(UDP_LEN));
    unsigned short UDP_CHECK_SUM = udp_packet.UDP_CHECK_SUM;
    memcpy(&buf[6], &UDP_CHECK_SUM, sizeof(UDP_CHECK_SUM));
    memcpy(&buf[8], UDP_Data, UDP_Data_Len);
    //返回字节数
    return 8 + UDP_Data_Len;
}

///////////////////////传输层-发送-END////////////////////////////////

///////////////////////测试-START////////////////////////////////
// Test
struct IP_Packet ip_packet_info = {0b0100, 0b1111, 0b00000000,         //IPv4_Version,IPv4_IHL,IPv4_TOS
                                   0b0000000000000000,                 //IPv4_TotalLength
                                   0b0000000000000000,                 //IPv4_Identification
                                   0b0, 0b0, 0b0, 0b0000000000000,     //IPv4_NoFunc,IPv4_DF,IPv4_MF,IPv4_FragmentOffset
                                   0b01010101, 0b10101010,             //IPv4_TimeTolive,IPv4_Protocol
                                   0b0000000000000000,                 //IPv4_HeaderCheckSum
                                   0b11011010010001101111111101100001, //IPv4_SourceAddr //218.70.255.97
                                   0b01110010001101110101111100001110, //IPv4_DesAddr //114.55.95.14
                                   0b0,                                //IPv4_Option
                                   0b0};                               //IPv4_Data
struct UDP_Packet udp_packet_info = {
    0b0000000000000111, 0b1110000000000000, //UDP_SRC_PORT,UDP_DES_PORT
    0b1111111111111111, 0b0000000000000000, //UDP_LEN,UDP_CHECK_SUM
    0b0};                                   //UDP_Data

class User
{
public:
    User(){};
    User(int _sock, int _id, char *_name, char *_password)
    {
        id = _id;
        sock = _sock;
        strcpy(name, _name);
        strcpy(password, _password);
    }
    int op;
    int id;
    int sock;
    char IP[15];
    char name[15];
    char password[20];
};

void printtime()
{
    time_t timeep;
    time(&timeep);
    printf("%s", ctime(&timeep));
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        puts("./cient <IP> <PORT> <ID> <PASSWORD> <OP>");
        exit(0);
    }
    fd_set server_fd;
    char send_buff[MAX_DATA_SIZE], recv_buff[MAX_DATA_SIZE];
    User client;
    if (atoi(argv[5]) == 0)
    {
        printf("please input your user_name: ");
        scanf("%s", client.name);
    }
    // Message send_message;
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    printf("%s loading...\n", argv[3]);
    if (connect(server_sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        exit(1);
    }
    client.op = atoi(argv[5]);
    client.id = atoi(argv[3]);
    strcpy(client.password, argv[4]);
    if ((write(server_sock, (char *)&client, sizeof(client))) <= 0)
    {
        perror("send message failed...");
        exit(1);
    }
    if (read(server_sock, recv_buff, MAX_DATA_SIZE) <= 0)
    {
        perror("read result failed...");
        exit(1);
    }
    if (atoi(argv[5]) == 1)
    {
        if (recv_buff[0] == 'n')
        {
            puts("please check user_name or user_password");
            exit(0);
        }
        puts("登录成功");
        puts("********************************************************");
        memset(recv_buff, 0, sizeof(recv_buff));
    }
    else
    {
        if (recv_buff[0] == 'n')
        {
            perror("register failed");
            exit(1);
        }
        else
        {
            printf("register succenss ans has login with user: %s\n", client.name);
        }
        memset(recv_buff, 0, sizeof(recv_buff));
    }
    int max_server_fd = 0;
    timeval timeout = {0};
    timeout.tv_usec = 500;
    while (true)
    {
        FD_ZERO(&server_fd);
        FD_SET(server_sock, &server_fd);
        max_server_fd = std::max(max_server_fd, server_sock);
        switch (select(max_server_fd + 1, &server_fd, NULL, NULL, &timeout))
        {
        case -1:
        {
            perror("select error");
            exit(1);
        }
        case 0:
            break;
        default:
        {
            if (FD_ISSET(server_sock, &server_fd))
            {
                ssize_t recv_size = read(server_sock, recv_buff, MAX_DATA_SIZE);
                if (recv_size <= 0)
                {
                    puts("server close");
                    close(server_sock);
                    exit(1);
                }
                printtime();
                //printf("recive message!!!!!!!\n");
                printf("%s\n", recv_buff);
                memset(recv_buff, 0, sizeof(recv_buff));
            }
        }
        }
        FD_ZERO(&server_fd);
        FD_SET(STDIN_FILENO, &server_fd);
        switch (select(STDIN_FILENO + 1, &server_fd, NULL, NULL, &timeout))
        {
        case -1:
        {
            perror("select error");
            exit(1);
        }
        case 0:
            break;
        default:
        {
            if (FD_ISSET(STDIN_FILENO, &server_fd))
            {
                ssize_t recv_len = read(STDIN_FILENO, recv_buff, MAX_DATA_SIZE);
                printf("main-recv_len: %d\n", (int)recv_len);

                if (recv_len <= 0)
                {
                    perror("read failed");
                    exit(1);
                }

                char address[1518];
                char message[1518];

                int lens = ((std::string)(recv_buff)).length();
                for (int ii = lens - 2; ii >= 0; ii--)
                {
                    if (((std::string)(recv_buff))[ii] == '-' && ((std::string)(recv_buff))[ii + 1] == '>')
                    {
                        //string sub1 = s.substr(5); //只有一个数字5表示从下标为5开始一直到结尾：sub1 = "56789"
                        //string sub2 = s.substr(5, 3); //从下标为5开始截取长度为3位：sub2 = "567"
                        strcpy(message, ((std::string)(recv_buff)).substr(0, ii).c_str());
                        strcpy(address, ((std::string)(recv_buff)).substr(ii + 2).c_str());
                        break;
                    }
                }
                lens = strlen(address);
                address[lens - 1] = '\0';
                printf("main-message: %s\n", message);
                printf("main-address: %s\n", address);

                //存放udp数据包
                unsigned char UDP_Buffer[UDP_DATA_MAXSIZE + 8];
                unsigned int UdpPacketLen = MakeUdpPacket(udp_packet_info, UDP_Buffer, (unsigned int)strlen(message), (unsigned char *)message);
                //将形成的UDP数据包发送给网络层处理
                network_layer_send(UDP_Buffer, UdpPacketLen, server_sock, address);

                memset(recv_buff, 0, sizeof(recv_buff));
            }
            break;
        }
        }
    }
    return 0;
}