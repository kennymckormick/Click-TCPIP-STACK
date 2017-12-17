#ifndef IP_HH
#define IP_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include "packets.hh"
#include <cstring>
#include <iostream>
using std::cout;
using std::endl;
#define MAXN 64

CLICK_DECLS

class IPElement : public Element {
	public:
		IPElement();
		~IPElement();
		const char *class_name() const { return "IPElement";}
		const char *port_count() const { return "1-/1-";}
		const char *processing() const { return PUSH;}
		int configure(Vector<String>&,ErrorHandler*);
		void push(int, Packet*);
		int initialize(ErrorHandler*);
		void run_timer(Timer* timer);
		void dijkstra(uint32_t addr);
		void erase_port(int);
	private:
		HashTable<unsigned,int> ip2port;
		HashTable<int,unsigned> port2ip;
		HashTable<unsigned,int> ipnxthop;
		uint32_t addr;
		int dist[MAXN+5][MAXN+5];
		bool port_used[MAXN+5];
		int port_age[MAXN+5];
		Timer timer_exchange;
		Timer timer_hello;
		Timer timer_timepass;
		int dt_hello;
		int dt_exchange;
		int dt_timepass;
		

		uint32_t iplist[MAXN+5];
		int ip_in_list(uint32_t x){
			for(int i=1; i<=MAXN; i++)
			if(x == iplist[i]) return i;
			return -1;
		} 
		int get_ip_id(uint32_t x){
			int tmp = ip_in_list(x);
			if(tmp != -1){
				return tmp;
			} else {
				int t = 1;
				while(iplist[t] != 0xFFFFFFFF && t <= MAXN) t++;
				if (t == MAXN + 1) {
					cout << "too many host in the network!!!" << endl;
					return -1;
				}
				return t;
			}
		}

};
CLICK_ENDDECLS
#endif

