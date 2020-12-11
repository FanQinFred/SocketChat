#include "Message.h"
#include <cstring>

Message::Message() {
    memset(address, 0, sizeof(address));
    memset(message, 0, sizeof(message));
}

Message::~Message() {
}

bool Message::setMessage(std::string _message) {
    int len = _message.length();
    printf("_message.length(): %d\n",len);
    for(int i = len - 2; i >= 0; i--) {
        if(_message[i] == '-' && _message[i + 1] == '>') {
            //string sub1 = s.substr(5); //只有一个数字5表示从下标为5开始一直到结尾：sub1 = "56789"
           //string sub2 = s.substr(5, 3); //从下标为5开始截取长度为3位：sub2 = "567"
            strcpy(message, _message.substr(0, i).c_str());
            strcpy(address, _message.substr(i + 2).c_str());
            break;
        }
    }
    len = strlen(address);
    address[len - 1] = '\0';
    printf("message: %s\n",message);
    printf("address: %s\n",address);
    printf("11111111\n");
    if(len == 0) return false;
    printf("22222222\n");
    for(int i = 0; i < len - 1; i++) { //len-1 for '\n'
        if(!isdigit(address[i])) {
            return false;
        }
    }
    return true;
}

void Message::setMessageWithLen( char* _message,unsigned int mlen, char* _address,unsigned int alen) {
    memcpy(message,_message,mlen);
    memcpy(address,_address,alen);
}

char * Message::getMessage() {
    char * ptr = message;
    return ptr;
}

int Message::getAddress() {
    return atoi(address);
}

void Message::clear() {
    memset(address, 0, sizeof(address));
    memset(message, 0, sizeof(message));
}
