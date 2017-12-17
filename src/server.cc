#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include "server.hh"
#include <fstream>
#include <string>
#include <iostream>
#include "packets.hh"
using std::string;
using std::ifstream;
using std::cout;
using std::endl;

CLICK_DECLS
int Server::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
                  "TYPE", cpkP+cpkM, cpUnsigned, &type,
                  "TEXT", cpkP+cpkM, cpString, &str,
                  "DEST_IP", cpkP+cpkM, cpIPAddress, &dst_ip,
                  "DEST_PORT", cpkP+cpkM, cpTCPPort, &dst_port,
                  cpEnd) < 0) {
        return -1;
    }
    return 0;
}

int Server::initialize(ErrorHandler* errh)
{
    const char* data = str.data();
    int len = str.length();
    if (type == MESSAGE) {
        WritablePacket* outpacket = Packet::make(sizeof(struct MyAPPHeader)+len);
        ((struct MyAPPHeader*)outpacket->data())->dst_ip = dst_ip;
        ((struct MyAPPHeader*)outpacket->data())->dst_port = dst_port;
        ((struct MyAPPHeader*)outpacket->data())->seq = 0;
        ((struct MyAPPHeader*)outpacket->data())->tot = 1;
        ((struct MyAPPHeader*)outpacket->data())->size = sizeof(struct MyAPPHeader) + len;
	cout << ((struct MyAPPHeader*)outpacket->data())->size << "<- APP packet size" << endl;
        memcpy((char *)(outpacket->data() + sizeof(struct MyAPPHeader)),data,len);
        output(0).push(outpacket);
    } else {
        string filename(data, len);
        ifstream is(filename.c_str(), ifstream::binary);
        click_chatter("Server reading file: %s", filename.c_str());
        is.seekg(0, is.end);
        uint32_t len = is.tellg();
        is.seekg(0, is.beg);
        int num_packet = (len - 1) / 500 + 1;
        for(int i=0;i<num_packet;i++){
            uint32_t cur_len = (i==num_packet-1)?(len%500):500;
            WritablePacket *outpacket = Packet::make(cur_len + sizeof(struct MyAPPHeader));
            ((struct MyAPPHeader*)outpacket->data())->dst_ip = dst_ip;
            ((struct MyAPPHeader*)outpacket->data())->dst_port = dst_port;
            ((struct MyAPPHeader*)outpacket->data())->seq = i;
            ((struct MyAPPHeader*)outpacket->data())->tot = num_packet;
            ((struct MyAPPHeader*)outpacket->data())->size = sizeof(struct MyAPPHeader) + cur_len;
            is.read((char*)(outpacket->data()+sizeof(struct MyAPPHeader)), cur_len);
            output(0).push(outpacket);
        }
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Server)
