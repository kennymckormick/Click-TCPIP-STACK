#include<click/config.h>
#include<click/confparse.hh>
#include<click/error.hh>
#include "ipelement.hh"
#include "packets.hh"
#include <queue>
#include <vector>
using std::queue;
using std::vector;
#define min(a,b) (a<b?a:b)
#define mem(a,b) memset(a,b,sizeof(a))
#define PORTCNT (this->nports(false))
#define rep(i,j,k) for(int i=j;i<=k;i++)
CLICK_DECLS
IPElement::IPElement():timer_hello(this),timer_exchange(this),timer_timepass(this){}
IPElement::~IPElement(){}
int IPElement::configure(Vector<String> &conf, ErrorHandler *errh) {
    if (cp_va_kparse(conf, this, errh,
                  "MY_ADDRESS", cpkP+cpkM, cpIPAddress, &addr,
                  "PERIOD_HELLO", cpkP, cpUnsigned, &dt_hello,
                  "PERIOD_EXCHANGE", cpkP, cpUnsigned, &dt_exchange,
                  cpEnd) < 0) {
        return -1;
    }
    dt_timepass = 5;
    return 0;
}
int IPElement::initialize(ErrorHandler* errh){
    port2ip[0] = addr;
    ip2port[addr] = 0;
    port_used[0] = true;
    mem(dist,0x3F);
    mem(iplist,0xFF);
    dist[0][0] = 0;
    iplist[0] = addr;
    timer_hello.initialize(this);
    timer_exchange.initialize(this);
    timer_timepass.initialize(this);
    timer_hello.schedule_now();
    timer_exchange.schedule_after_sec(dt_exchange);
    timer_timepass.schedule_after_sec(dt_timepass);
    return 0;
}
void IPElement::erase_port(int port){
    //click_chatter("200: MY IP %x, ERASE PORT: %d",addr, port);
    uint32_t ip = port2ip[port];
    ip2port.erase(ip);
    port2ip.erase(port);
    port_age[port] = 0;
    port_used[port] = false;
    vector<uint32_t> ipvec;
    rep(i,1,MAXN){
        if(ipnxthop[iplist[i]] == port){
            uint32_t ip_tmp = iplist[i];
	    ipvec.push_back(ip_tmp);
            iplist[i] = 0xFFFFFFFF;
            ipnxthop.erase(ip_tmp);
            rep(j,0,MAXN){
                dist[i][j] = 0x3F3F3F3F;
                dist[j][i] = 0x3F3F3F3F;
            }
        }
    }
    int len =  ipvec.size();
    WritablePacket *fail = Packet::make(NULL,sizeof(struct MyIPHeader)+4*len);
    memset(fail->data(),0,fail->length());
    struct MyIPHeader* format = (struct MyIPHeader*)(fail->data());
    format -> type = FAILURE;
    format -> TTL = 1;
    format -> src = this -> addr;
    format -> dest = 0xffffffff;
    format -> size = sizeof(struct MyIPHeader)+4*len;
    char* data_c = (char*)(fail->data()+sizeof(struct MyIPHeader));
    uint32_t* data = (uint32_t*)data_c;
    for(int i=0;i<len;i++)
    data[i] = ipvec[i];
    rep(i,1,PORTCNT-1)
    if(i!=port)
    output(i).push(fail);
}

void IPElement::run_timer(Timer* timer){
    //click_chatter("001: Enter run_timer of %x ", addr);
    struct MyIPHeader* format;
    if(timer == &timer_hello){
        rep(i,1,PORTCNT - 1){
	    //click_chatter(" ");
            WritablePacket *hello = Packet::make(NULL,sizeof(struct MyIPHeader));
            memset(hello->data(),0,hello->length());
            format = (struct MyIPHeader*)(hello->data());
            format -> type = HELLO;
            format -> TTL = 1;
            format -> src = this -> addr;
            format -> dest = 0xffffffff;
            format -> size = sizeof(struct MyIPHeader);
            output(i).push(hello);
        }
	//click_chatter("002: My IP is %x, I just broadcast Hello message to %d ports", this->addr, PORTCNT - 1);
        timer->reschedule_after_sec(dt_hello);
    } else if (timer == &timer_exchange) {
        int cnt = 0;
        rep(i,0,MAXN) rep(j,i+1,MAXN) {
            if (dist[i][j] != 0x3F3F3F3F)
            cnt ++;
        }
	//click_chatter("003: My IP is %x, I just broadcast Exchange message to %d ports", this->addr, PORTCNT - 1);
        rep(i,1,PORTCNT - 1)
        if(1){
            WritablePacket *exchange = Packet::make(NULL,sizeof(struct MyIPHeader) + 12 * cnt);
            memset(exchange->data(),0,exchange->length());
            format = (struct MyIPHeader*)(exchange->data());
            format -> type = EXCHANGE;
            format -> TTL = 5;
            format -> src = this -> addr;
            format -> dest = 0xffffffff;
            format -> size = sizeof(struct MyIPHeader) + 12 * cnt;
            char* data_c = (char*)(exchange->data()+sizeof(struct MyIPHeader));
            uint32_t* data = (uint32_t*)data_c;
            int count = 0;
            rep(ii,0,MAXN) rep(jj,ii+1,MAXN) {
                if (dist[ii][jj] != 0x3F3F3F3F) {
                    data[3*count] = iplist[ii];
                    data[3*count+1] = iplist[jj];
                    data[3*count+2] = dist[ii][jj];
               	    count++; 
		    if (i == 1) {
		    	//click_chatter("EXCHANGE PACKET CONTAINS: SRC: %x; DEST: %x; DIST: %d",iplist[ii],iplist[jj],dist[ii][jj]);
		    }
		}
            }
            output(i).push(exchange);
        }
        timer->reschedule_after_sec(dt_exchange);
    } else if (timer == &timer_timepass) {
        rep(i,1,PORTCNT-1){
            if (port2ip[i]) port_age[i] += 5;
	    //click_chatter("IP: %x, PORT: %d, PORT_AGE: %d",addr,i,port_age[i]);
            if (port_age[i] >= 20) {
                erase_port(i);
            }
        }
	//click_chatter("004: My IP is %x, I will check if I should erase my %d ports", this->addr, PORTCNT - 1);
	timer->reschedule_after_sec(dt_timepass);
    } else {
        cout << "invalid timer" << endl;
    }
}
void IPElement::push(int port,Packet* packet){
    // we get packet from tcp
    
    if (port == 0) {
        struct MyTCPHeader* tcp_header = (struct MyTCPHeader*)(packet->data());
        if(!ipnxthop[tcp_header->dest]){
            cout << "unknown destination" << endl;
            packet -> kill();
            return;
        } 
        WritablePacket *ip_encap = Packet::make(NULL,sizeof(struct MyIPHeader) + tcp_header->size);
        struct MyIPHeader* format = (struct MyIPHeader*)(ip_encap->data());
        format -> type = DATA;
        format -> TTL = 30;
        format -> src = addr;
        format -> dest = tcp_header->dest;
        format -> size = sizeof(struct MyIPHeader) + tcp_header -> size;
        char* data = (char*)(ip_encap->data() + sizeof(struct MyIPHeader));
        memcpy(data, tcp_header, tcp_header->size);
        int out_port = ipnxthop[tcp_header->dest];
        output(out_port).push(ip_encap);
    } else {
        struct MyIPHeader* ip_header = (struct MyIPHeader*)(packet->data());
        if (ip_header -> type == HELLO) {
            WritablePacket* response = Packet::make(NULL,sizeof(struct MyIPHeader));
            struct MyIPHeader* format = (struct MyIPHeader*)(response->data());
            format -> type = RESP;
            format -> TTL = 1;
            format -> src = this -> addr;
            format -> dest = ip_header -> src;
            format -> size = sizeof(struct MyIPHeader);
            packet -> kill();
            output(port).push(response);
	    //click_chatter("101:MYIP IS :%x  AND I RECIEVE HELLO FROM IP %x PORT %d, AND SEND BACK A RESP", addr, (int)(ip_header->src), port);
            return;
        }
        if (ip_header -> type == RESP){
            port_used[port] = true;
            port_age[port] = 0;
            if (!ip2port[ip_header->src]) {
                ip2port[ip_header->src] = port;
                port2ip[port] = ip_header->src;
		ipnxthop[ip_header->src] = port;
                int ip_id = ip_in_list(ip_header->src);
                if(ip_id != -1){
                    dist[ip_id][0] = 1;
                    dist[0][ip_id] = 1;
                } else {
                    ip_id = get_ip_id(ip_header->src);
		    iplist[ip_id] = ip_header->src;
                    dist[ip_id][0] = 1;
                    dist[0][ip_id] = 1;
                }
		//click_chatter("301: RENEW PAIR: MYIP %x,DEST %x",addr,ip_header->src);
            }
	    //click_chatter("102:MYIP IS :%x  AND I RECIEVE RESP FROM IP %x PORT %d, RENEW MY DEST TABLE", addr, (int)(ip_header->src),port);
            //dijkstra(addr);
	    packet->kill();
            return;
        }
	if (ip_header -> type == FAILURE){
	    char* data_c = (char*)(packet->data() + sizeof(struct MyIPHeader));
	    int length = ip_header -> size - sizeof(struct MyIPHeader);
	    length /= 4;
	    uint32_t* data = (uint32_t*) data_c;
	    vector<uint32_t> ipvec;
	    rep(i, 0, length - 1){
		uint32_t del_ip = data[i];
		if(ip_in_list(del_ip)!=-1 && ipnxthop[del_ip]==port){
		    ipvec.push_back(del_ip);
		    uint32_t del_id = ip_in_list(del_ip);
		    rep(i,0,MAXN) dist[del_id][i]=dist[i][del_id]=0x3F3F3F3F;
		    ipnxthop.erase(del_ip);
		    iplist[del_id]=0xFFFFFFFF;
		}
	    }
	    if(ipvec.size() != 0){
		int len =  ipvec.size();
	    	WritablePacket *fail = Packet::make(NULL,sizeof(struct MyIPHeader)+4*len);
    		memset(fail->data(),0,fail->length());
    		struct MyIPHeader* format = (struct MyIPHeader*)(fail->data());
    		format -> type = FAILURE;
    		format -> TTL = 1;
    		format -> src = this -> addr;
    		format -> dest = 0xffffffff;
    		format -> size = sizeof(struct MyIPHeader)+4*len;
    		char* data_c = (char*)(fail->data()+sizeof(struct MyIPHeader));
    		uint32_t* data = (uint32_t*)data_c;
    		for(int i=0;i<len;i++)
    		data[i] = ipvec[i];
    		rep(i,1,PORTCNT-1)
    		if(i!=port)
    		output(i).push(fail);
	    }
	    packet->kill();
	    return;
    	}

        if (ip_header -> type == EXCHANGE){
	    //click_chatter("103:MYIP IS :%x  AND I RECIEVE EXCHANGE FROM IP %x PORT %d", addr, (int)(ip_header->src), port);
            port_used[port] = true;
            port_age[port] = 0;
            char* data_c = (char*)(packet->data() + sizeof(struct MyIPHeader));
            int length = ip_header -> size - sizeof(struct MyIPHeader);
            length /= 12;
	    //click_chatter("EXCHANGE PACKET RECV: LENGTH %d",length);
            uint32_t* data = (uint32_t*) data_c;
            rep(i, 0, length - 1){
                uint32_t src_ip = data[3*i], dest_ip = data[3*i+1];
                int d = data[3*i+2];
		//click_chatter("EXCHANGE PACKET RECV: SRC %x, DEST: %x, DIST: %d", src_ip, dest_ip, d);
		if(src_ip == addr || dest_ip == addr) continue;
                int src_id = ip_in_list(src_ip);
                if (src_id == -1) { 
		    src_id = get_ip_id(src_ip);
		    iplist[src_id] = src_ip;
		}
                int dest_id = ip_in_list(dest_ip);
                if (dest_id == -1) {
		    dest_id = get_ip_id(dest_ip);
		    iplist[dest_id] = dest_ip;
		}
                if (dist[src_id][dest_id] > d) {
                    dist[src_id][dest_id] = d;
                    dist[dest_id][src_id] = d;
                }
	    }

            dijkstra(addr);
	    ip_header->TTL--;
	    if (ip_header->TTL != 0){
		rep(i,1,PORTCNT-1)
		if(i != port && port_used[i]){
		    output(i).push(packet);
		    //click_chatter("103:MYIP IS :%x  AND I RECIEVE EXCHANGE FROM IP %x PORT %d, FORWARD IT TO PORT %d", addr, (int)(ip_header->src), port, i);
		}
	    }
	    packet->kill();
	    return;	
        }
		
        if(ip_header -> type == DATA){
            port_used[port] = true;
            port_age[port] = 0;
            if (ip_header -> dest == this -> addr){
                WritablePacket* tcppacket = Packet::make(packet->data()+sizeof(MyIPHeader), ip_header->size - sizeof(MyIPHeader));
                output(0).push(tcppacket);
                return;
            } else {
		if (ip_header -> TTL == 1) {
			packet->kill();
			return;
		}
		ip_header->TTL-=1;
                if (ipnxthop[ip_header->dest]){
	                output(ipnxthop[ip_header->dest]).push(packet);
			//click_chatter("104:MYIP IS :%x  AND I RECIEVE DATA FROM IP %x PORT %d, FORWARD IT TO PORT %d", addr, (int)(ip_header->src ),port, ipnxthop[ip_header->dest]);
		}
                else
                cout << "failed to forward packet because destination is unreachable" << endl;
            }
            packet->kill();
            return;
        }
 	click_chatter("UNKNOWN PACKET: TYPE %d, SRC %x, DEST %x,TTL %x,Size %d",ip_header->type,ip_header->src,ip_header->dest,ip_header->TTL,ip_header->size);	
        return;
    }
}
void IPElement::dijkstra(uint32_t addr){
    int d[MAXN + 5];
    mem(d,0xFF);
    rep(i,0,MAXN) dist[i][i] = 0;
    rep(i,0,MAXN) rep(j,i+1,MAXN) {
        dist[i][j] = dist[j][i] = min(dist[i][j], dist[j][i]);
    }
    rep(k,0,MAXN) rep(i,0,MAXN) rep(j,0,MAXN) {
        if(dist[i][j] > dist[i][k] + dist[k][j]) dist[i][j] = dist[i][k] + dist[k][j];
    }
    //as here we assume the distance of all connected pair is 1, we can use BFS to replace dijkstra
    queue<int> q;
    while(!q.empty()) q.pop();
    q.push(0);
    d[0] = 0;
    while(!q.empty()){
        int cur = q.front();
        q.pop();
        rep(i,1,MAXN){
            if(d[i] == -1 && dist[cur][i] == 1){
                d[i] = d[cur] + 1;
		q.push(i);
		//click_chatter("CUR: %d, PRE_IP: %x, CUR_IP: %x, IP2PORT: %d, IP_PRE_NHOP: %d",cur,iplist[cur],iplist[i],ip2port[iplist[i]],ipnxthop[iplist[cur]]);
                if(cur == 0)
                ipnxthop[iplist[i]] = ip2port[iplist[i]];
                else
                ipnxthop[iplist[i]] = ipnxthop[iplist[cur]];
            } 
        }
    }
    //click_chatter("400: AFTER DIJK: WE(IP: %x) ARE CONNECTED TO THE FOLLOWING IP", addr);
    for(int i=1;i<=MAXN;i++){
	if(iplist[i]!=-1);
	//click_chatter("IP: %x;  NEXTHOP: %d",iplist[i],ipnxthop[iplist[i]]);
    }
}


CLICK_ENDDECLS 
EXPORT_ELEMENT(IPElement)
