#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include "TCPElement.hh"
#include <vector>
#include<iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using std::vector;
using std::cout;
using std::endl;
#define min(a, b) (a<b?a:b)
#define mem(a, b) memset(a,b,sizeof(a))
#define PORTCNT (this->nports(false))
#define rep(i, j, k) for(int i=j;i<=k;i++)
int cntt = 0;
CLICK_DECLS

TCPElement::TCPElement() :timer_resend(this), timer_debug(this),timer_fin(this),client_submit_timer(this) {
}

TCPElement::~TCPElement() {}

int TCPElement::configure(Vector <String> &conf, ErrorHandler *errh) {
    if (cp_va_kparse(conf, this, errh, "MY_PORT", cpkP + cpkM, cpTCPPort,
                     &myport, "MY_IP", cpkP + cpkM, cpIPAddress, &myip, "TYPE",
                     cpkP + cpkM, cpUnsigned, &tcpele_type, "TIME_OUT",
                     cpkP + cpkM, cpUnsigned, &timeout, cpEnd) < 0)
        return -1;
    return 0;
}

int TCPElement::initialize(ErrorHandler *) {
    timer_resend.initialize(this);
    timer_fin.initialize(this);
    client_submit_timer.initialize(this);
    nxt_ack = nxt_seq = 0;
    coport = coip = 0;
    cur_data = base_data = 0;
    cwnd = 1;
    lost = 0;
    acc_succeed = 0;
    last_ack = 0;
    acked_max = 0;
    state = CLOSED;
    memset(slot,0,sizeof(slot));
    std::srand(std::time(0));
    return 0;
}

void TCPElement::set_timer(Packet *packet,uint16_t size) {
    info_resend = Packet::make(packet->data(),size);
    timer_resend.schedule_after_sec(timeout);
}

void TCPElement::set_fin_timer(){
    timer_fin.schedule_after_sec(4*timeout);
}

void TCPElement::set_client_timer(){
    client_submit_timer.schedule_after_sec(3);
}

void TCPElement::cancel_timer() {
    /*
    if (info_resend.timer_stat == tinfo.timer_stat &&
        info_resend.timer_seq == tinfo.timer_seq &&
        info_resend.timer_ack == tinfo.timer_ack)
        */
    timer_resend.unschedule();
}


void TCPElement::cancel_fin_timer() {
    /*
    if (info_resend.timer_stat == tinfo.timer_stat &&
        info_resend.timer_seq == tinfo.timer_seq &&
        info_resend.timer_ack == tinfo.timer_ack)
        */
    timer_fin.unschedule();
}

void TCPElement::cancel_client_timer(){
    client_submit_timer.unschedule();
}

void TCPElement::push(int port, Packet *packet) {
    if (port == 1) {
        // from app level recieve data, suppose data is already devided into
        // appropriate size
        // which means if app level decide to send a file, a bunch of packets
        // flow into port 1, accumulated in buffer
        // you need to establish connection (send SYN) and change the state

        //need to buffer...
        WritablePacket* tmp = Packet::make(NULL,packet->length());
	memcpy(tmp->data(),packet->data(),packet->length());
        buffer.push(tmp);
        click_chatter("TCP Element received packet from APP");
        click_chatter("There are %d packets in the queue now", buffer.size());
        //now we don't know coIP or coPort
        //we neet get it from the packet..
        struct MyAPPHeader *last = (struct MyAPPHeader *) (packet->data());
        coip = last->dst_ip;
        coport = last->dst_port;
        if (state == CLOSED) {

            WritablePacket *tcp_syn = Packet::make(NULL, sizeof(struct MyTCPHeader));
            struct MyTCPHeader *format = (struct MyTCPHeader *) (tcp_syn->data());
            //establishing.. so seq number zero.
            // or maybe we need some randomization?
            nxt_ack = 0;
            nxt_seq = 0;
            makeTCPhead(format, SYN);
            click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                          myport, coip, coport);
            click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);
            state = SYN_SENT;
            nxt_ack ++;
            nxt_seq ++;
            set_timer(tcp_syn,format->size);
            output(0).push(tcp_syn);
            return;
        }
    } else if (port == 0) {
        // our model: server get data from app level and start a request, send
        // data to the client
		
        uint8_t type = ((MyTCPHeader*)packet->data())->type;
		uint32_t seq = ((MyTCPHeader*)packet->data())->seq;
		uint32_t ack = ((MyTCPHeader*)packet->data())->ack;
		uint16_t sport = ((MyTCPHeader*)packet->data())->src_port;
		uint16_t dport = ((MyTCPHeader*)packet->data())->dest_port;
		uint32_t sip = ((MyTCPHeader*)packet->data())->src;
        if (tcpele_type == 0) {  // which means you are a client
	if (rand()%100==3){
		click_chatter("I am %d,I lost packet with seq %d,ack %d",tcpele_type,seq,ack);
		packet->kill();
		return;
	}
            if (state == CLOSED) {
                // server start request, client receive it, send SYN+ACK to the
                // server
                if (((struct MyTCPHeader *) packet->data())->type != SYN) {
                    //packet should be TCP SYN packet
                    click_chatter("I'm client and I'm waiting for a syn packet from server, get the wrong one");
                    packet->kill();
                    return;
                }

                WritablePacket *tcp_synack = Packet::make(NULL, sizeof(struct MyTCPHeader));
                struct MyTCPHeader *format = (struct MyTCPHeader *) (tcp_synack->data());
                struct MyTCPHeader *last = (struct MyTCPHeader *) (packet->data());
                coip = last->src;
                coport = last->src_port;
                makeTCPhead(format, SYN+ACK);
                format->slot = CLIENT_BUFFER_SIZE;
                click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                              myport, coip, coport);
                click_chatter("sent pack with seq: %d ,ack %d", 0, last->seq + 1);
                acked_max ++;

                format->ack = last->seq + 1;
                format->seq = 0;  //establishing.. so seq number zero.
                // or maybe we need some randomization?

                state = SYN_WAIT;
                set_timer(tcp_synack,format->size);
                nxt_ack = 2;
                nxt_seq ++;
                output(0).push(tcp_synack);
                return;
            }
            if (state == SYN_WAIT) {
                // sent a SYN+ACK to server, RECEIVE ACK again
                // change state to connected
                if (((struct MyTCPHeader *) packet->data())->type != ACK) {
                    click_chatter("I'm client, I'm, waiting for an ACK packet to establish the connection, get wrong packet!");
                    packet->kill();
                    return;
                }
                //ack number needs to be nxt seq
                struct MyTCPHeader *last = (struct MyTCPHeader *) (packet->data());
                if (last->ack != nxt_seq) {
                    click_chatter("I'm client, I get ACK but with the wrong ACK number!");
                    packet->kill();
                    return;
                }
                cancel_timer();
                //与三步握手不同
                //我们需要第四步回传syn&ack来让sender发包
                WritablePacket *tcp_synack = Packet::make(NULL, sizeof(struct MyTCPHeader));
                struct MyTCPHeader *format = (struct MyTCPHeader *) (tcp_synack->data());
                state = CONNECTED;
                makeTCPhead(format, ACK);
                format->slot = CLIENT_BUFFER_SIZE;
                acked_max ++;
                click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                              myport, coip, coport);
                click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);
                set_timer(tcp_synack,format->size);
		format->ack = seq + 1;
                nxt_ack = nxt_ack + 1;
                nxt_seq = nxt_seq + 1;
                output(0).push(tcp_synack);
                return;
            }
            if (state == CONNECTED) {
                // we receive data packet
                // send data to the app level, send ACK to the server
                struct MyTCPHeader *last = (struct MyTCPHeader *) (packet->data());
                if (type != FIN && type != DATA){
                    click_chatter("I'm a connected client but I get the wrong packet!");
		            click_chatter("I get a packet with type %d",type);
                    packet->kill();
                    return;
                }
		click_chatter("receiver received packet, with seq %d, ack %d",seq,ack);
                if (last->seq != nxt_seq) {
		    
		    struct MyTCPHeader* tcp_header = (struct MyTCPHeader *) (packet->data());
                    if (last->seq > nxt_seq && last->seq < nxt_seq + CLIENT_BUFFER_SIZE)
                    slot[last->seq - nxt_seq] =  Packet::make(packet->data() + sizeof(struct MyTCPHeader),
                                                            tcp_header->size - sizeof(struct MyTCPHeader));
		    if (last->seq >= nxt_seq + CLIENT_BUFFER_SIZE){
			packet->kill();
			return;
		    }	
                    WritablePacket *ackpacket = Packet::make(NULL, sizeof(struct MyTCPHeader));
                    struct MyTCPHeader *format = (struct MyTCPHeader *) (ackpacket->data());
                    makeTCPhead(format,ACK);
                    format->seq --; format->ack --;
		    if(last->seq < nxt_seq) {
			format->seq = last->seq;
			format->ack = last->ack+1;
		    }
                    if(last->seq + 1 >= nxt_ack + CLIENT_BUFFER_SIZE) format->slot = 0;
		    click_chatter("receive send back ACK, with seq %d, ack %d",format->seq,format->ack);
                    output(0).push(ackpacket);
                    packet->kill();
                    return;
                }
                cancel_timer();
                acked_max ++;
                if (type == DATA){
                    struct MyTCPHeader* tcp_header = (struct MyTCPHeader *) (packet->data());
                    slot[0] = Packet::make(packet->data() + sizeof(struct MyTCPHeader),
                                                            tcp_header->size - sizeof(struct MyTCPHeader));
                    WritablePacket *ackpacket = Packet::make(NULL, sizeof(struct MyTCPHeader));
                    struct MyTCPHeader *format = (struct MyTCPHeader *) (ackpacket->data());
                    click_chatter("TCP: connection established..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                                myport, coip, coport);
                    click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);
                    makeTCPhead(format, ACK);
                    int k = 0;
                    while(slot[k]) k++;
		    if (k > CLIENT_BUFFER_SIZE)  k = CLIENT_BUFFER_SIZE;
		    click_chatter("finally we got k to be %d",k);
		    for(int i=0;i<k;i++) output(1).push(slot[i]);
                    nxt_ack = format->ack + k;
                    nxt_seq = format->seq + k;
                    for(int i=0;i+k<CLIENT_BUFFER_SIZE;i++){
                        slot[i] = slot[i + k];
                    }
                    for(int j=CLIENT_BUFFER_SIZE-k;j<CLIENT_BUFFER_SIZE;j++) slot[j] =NULL;
		    format->count = k;
		    click_chatter("CLIENT DATA:send out ack with seq %d,ack %d,count %d",format->seq,format->ack,format->count);
                    set_timer(ackpacket,format->size);
                    output(0).push(ackpacket);
                    packet->kill();
                    return;
                }
                if (type == FIN){
                    click_chatter("TCP : recieved FIN from server!");
		click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                          myport, coip, coport);
            	click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);

		    WritablePacket *ack_pack = Packet::make(NULL,sizeof(struct MyTCPHeader));
		    struct MyTCPHeader* format = (struct MyTCPHeader*)(ack_pack->data());
                    makeTCPhead(format,FIN+ACK);
		    set_timer(ack_pack,format->size);
		    set_fin_timer();
            cancel_client_timer();
		    nxt_seq ++;
                    nxt_ack ++;
                    state = FIN_WAIT;
		    output(0).push(ack_pack);
                    packet->kill();
		    click_chatter("chunk of FIN finished");
                    return;
                }
            }
            if (state == FIN_WAIT){
                if(type != ACK){
					click_chatter("Client waiting for ACK for FIN, get wrong packet!");
					packet->kill();
					return;
				}
				else{
					click_chatter("Client received ACK for FIN, connection down!");
					cancel_timer();
					cancel_fin_timer();
					nxt_ack = nxt_seq = 0;
					coip = coport = 0;
					state = CLOSED;
					packet->kill();
                    return;
				}
            }
        }
        if (tcpele_type == 1) {  // which means you are a server
            click_chatter("I'm server, I received packet from IP");
            if (state == CLOSED) {
                // won't happen， cuz server start the connection
                packet->kill();
                return;
            }
            if (state == SYN_SENT) {
                // receive SYN+ACK from client, send ACK to client
                // shift state to CONNECTED
                struct MyTCPHeader *last = (struct MyTCPHeader *) (packet->data());
                if (last->type != ACK+SYN) {
                    click_chatter("I'm server, I'm waiting for SYN+ACK from client, get the wrong one!");
                    packet->kill();
                    return;
                }
                cancel_timer();
                WritablePacket *tcp_ack = Packet::make(NULL, sizeof(struct MyTCPHeader));
                struct MyTCPHeader *format = (struct MyTCPHeader *) (tcp_ack->data());
                click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                              myport, coip, coport);
                click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);

                makeTCPhead(format, ACK);
                state = CONNECTED;
                set_timer(tcp_ack,format->size);
                nxt_ack = nxt_ack + 1;
                nxt_seq = nxt_seq + 1;
                acked_max ++;
                output(0).push(tcp_ack);
                packet->kill();
                return;
            }
            if (state == CONNECTED) {
                // get ACK from client again
                // begin to send data to the client (packets in buffer)

                struct MyTCPHeader *last = (struct MyTCPHeader *) (packet->data());
                click_chatter("Server received packet from client");
		click_chatter("now ACKED max is %d, ACK'S ACK is %d",acked_max,last->ack);
                if ((last->type != ACK) ||(last->ack != acked_max + 1)) {
                    click_chatter("Server waiting for ACK, get wrong packet or not appropriate ACK");
		    click_chatter("Server received packet with seq %d,ack %d",seq,ack);
                    if(1) {
			last_ack ++;
			click_chatter("current last_ack %d",last_ack);
			if(last_ack >= 3){
			    Packet* data = server_buffer.front();
			    MyTCPHeader* format = (struct MyTCPHeader*)(data->data());
                            click_chatter("three times, retransmission");
			    click_chatter("retransmission packet is with seq %d,ack %d",format->seq,format->ack);
			    click_chatter("already popped out %d packets",cntt);
			    output(0).push(data);
                            last_ack = 0;
			}
                    }
                    packet->kill();
                    return;
                }
                if (last->slot == 0){
                    click_chatter("No more room in clients buffer, wait for a while");
                    packet->kill();
                    return;
                }
                cancel_timer();
                acc_succeed ++;
                if(lost >= 2) cwnd++;
                if(lost < 2 && acc_succeed == 2*cwnd - 1) cwnd*=2;
                if(cwnd > 1024) cwnd=1024;
                click_chatter("after a successful transimission, cwnd is increased to %d",cwnd);
                if(last->count == 0)
                acked_max ++;
                else
                acked_max += last->count;
                if(server_buffer.size()) {
                    if(last->count == 0)
                    server_buffer.pop();
                    else
                    for(int i=0;i<last->count;i++)
                    server_buffer.pop();
		    cntt += std::max((int)(last->count),1);
                    //in server buffer means already tranmitted
                }
                //TODO:
                //异步发送？
                //e.g. 先收到了ack
                //再buff了app层给到的包

                //stop&wait
                
                if (!buffer.empty()) {
                    while ((!buffer.empty()) && server_buffer.size() < cwnd){
                        Packet *datapacket = buffer.front();
                        buffer.pop();
                        //retransmission?
                        WritablePacket *tcp_encap = Packet::make(NULL, sizeof(struct MyTCPHeader) +
                                                                    ((struct MyAPPHeader *) (datapacket->data()))->size);
                        struct MyTCPHeader *format = (struct MyTCPHeader *) (tcp_encap->data());
                        click_chatter("TCP: connection established..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                                    myport, coip, coport);
                        click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);
                        makeTCPhead(format, DATA);
                        format->size = sizeof(struct MyTCPHeader) + ((struct MyAPPHeader *) (datapacket->data()))->size;
                        char *data = (char *) (tcp_encap->data() + sizeof(struct MyTCPHeader));
                        memcpy(data, datapacket->data(), ((struct MyAPPHeader *) (datapacket->data()))->size);
                        
                        nxt_seq++;
                        nxt_ack++;
                        WritablePacket *wp = Packet::make(NULL,tcp_encap->length());
			memcpy(wp->data(),tcp_encap->data(),tcp_encap->length());
                        server_buffer.push(wp);
                        output(0).push(tcp_encap);
                    }
		    Packet* pack = server_buffer.front();
                    set_timer(pack,pack->length());
                    return;

                } else {
				click_chatter("TCP: connection establishing..\n MY IP %x, MY PORT: %d, CO IP: %x, CO PORT: %d", myip,
                          		myport, coip, coport);
	 	           	click_chatter("sent pack with seq: %d ,ack %d", nxt_seq, nxt_ack);

                    if(nxt_seq == acked_max){
                        WritablePacket *fin_pack = Packet::make(NULL,sizeof(struct MyTCPHeader));
                        struct MyTCPHeader* format = (struct MyTCPHeader*)(fin_pack->data());
                        click_chatter("Server: data transportation finished, send FIN packet!");
                        makeTCPhead(format, FIN);
                        set_timer(fin_pack, format->size);
                        state = FIN_SENT;
                        output(0).push(fin_pack);
			            nxt_seq ++;
			            nxt_ack ++;
                    } else {
			Packet* pack = server_buffer.front();
                        set_timer(pack,pack->length());
                    }
                    packet->kill();
                    return;
                }
            }
            if (state == FIN_SENT) {
				click_chatter("Server in state fin_sent");
				cancel_timer();		
				if(type != ACK+FIN) 
				{
					click_chatter("Server: waiting for FIN+ACK from the client, get the wrong packet!");
                    			packet->kill();
					return;
				}
				WritablePacket *ack_pack = Packet::make(NULL,sizeof(MyTCPHeader));
				struct MyTCPHeader* format = (struct MyTCPHeader*)(ack_pack->data());
                		makeTCPhead(format,ACK);
                acked_max ++;
				output(0).push(ack_pack);
				packet->kill();
				state=CLOSED;
				nxt_seq = nxt_ack = 0;
				coip = coport = 0;
                return;
            }
        }
        // put flow control and congestion control in two CONNECTED state
    }
}

void TCPElement::run_timer(Timer *timer) {
    if (timer == &timer_debug) {
        // cout debug information, like state
        // reschedule timer
    } else if (timer == &timer_resend) {
        // resend packet
        click_chatter("timer resend fired, retransmitting packet, the entity is %d",tcpele_type);
	MyTCPHeader* format = (struct MyTCPHeader*)(info_resend->data());
	click_chatter("packet info: seq %d,ack %d",format->seq,format->ack);
        lost ++;
        acc_succeed = 0;
        cwnd /= 2;
	if(cwnd == 0)	cwnd = 1;
        output(0).push(info_resend);
        if (state == CLOSED)
            timer_resend.unschedule();
        else
            timer_resend.reschedule_after_sec(timeout);
            // reschedule timer
    } else if (timer == &timer_fin){
        click_chatter("timer fin fired, stop connection");
        cancel_timer();
        nxt_ack = nxt_seq = 0;
        state = CLOSED;
    }/* else if (timer == &client_submit_timer){
        click_chatter("timer client fired, submiting packets");
	    for(int i = 0; i < cur_data - base_data; i++){
            output(1).push(slot[i]);
        }
        base_data = cur_data;
        WritablePacket* reset = Packet::make(info_resend->data(),info_resend->length());
        struct MyTCPHeader* format = (struct MyTCPHeader*)(reset->data());
        format->slot = CLIENT_BUFFER_SIZE;
        set_timer(reset,format->size);	
        click_chatter("ACK slot now is %d",((struct MyTCPHeader*)info_resend)->slot);
        client_submit_timer.reschedule_after_sec(3);
    } */
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPElement)


