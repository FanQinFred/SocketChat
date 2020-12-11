#include <stdio.h>
#include <stdlib.h>
#include <string>
class Message {
public:
    Message ();
    ~Message();
    bool setMessage(std::string _message);
    void setMessageWithLen( char* _message,unsigned int mlen, char* _address,unsigned int alen);
    char * getMessage();
    int getAddress();
    void clear();
private:
    char address[1518];
    char message[1518];
};
