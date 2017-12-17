#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include "client.hh"
#include <fstream>
#include <string>
#include <iostream>
#include <cstring>
#include "packets.hh"
using std::string;
using std::ofstream;
using std::cout;
using std::endl;
ofstream os("tmp.txt",ofstream::binary);
CLICK_DECLS



int Client::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                  "TYPE", cpkP+cpkM, cpUnsigned, &type,
                  cpEnd) < 0) {
        return -1;
    }
    return 0;
}

int Client::initialize(ErrorHandler* errh){
    memset(flag,0,sizeof(flag));
}

void Client::push(int port, Packet* packet){
    if (type == MESSAGE) {
	click_chatter("I'm client, data received!!!");
        char message[1024];
        int len = ((struct MyAPPHeader*)packet->data())->size - sizeof(struct MyAPPHeader);  
	cout << len << endl;      
        memcpy(message,(char*)(packet->data() + sizeof(struct MyAPPHeader)),len);
        message[len] = 0;
        click_chatter("I'm client, and I get message ###%s### from server",message);
        packet->kill();
    } else if (type == DATA) {
	click_chatter("I'm client, and I receive data from server");
        uint16_t id = ((struct MyAPPHeader*)packet->data())->seq;
        uint16_t size = ((struct MyAPPHeader*)packet->data())->size;
        uint16_t tot = ((struct MyAPPHeader*)packet->data())->tot;
	if (id == tot - 1) 	tail = size - sizeof(struct MyAPPHeader);
        flag[id] = 1;
	memcpy((char*)buffer + 500*id, (char*)(packet->data()+sizeof(struct MyAPPHeader)), size - sizeof(struct MyAPPHeader));
	click_chatter("packet %d buffered",id);
        if(id == cnt) {
            while(flag[id]){
                id ++;
                cnt ++;
            }
        } 
        if(cnt == ((struct MyAPPHeader*)packet->data())->tot){
	    ofstream os("tmp.txt",ofstream::binary);
	    os.write(buffer,500*(cnt-1) + tail);
            os.close();
            click_chatter("I'm client, and I saved file!");
        }
	packet->kill();
    }
    click_chatter("I will quit");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Client)
