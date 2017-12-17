#ifndef CLIENT_HH
#define CLIENT_HH
#include <click/element.hh>
#define MESSAGE 0
#define DATA 1

CLICK_DECLS

class Client: public Element {
    public: 
        const char *class_name() const { return "Client";}
        const char *port_count() const { return "1/0"; }
        const char *process() const { return PUSH; }
        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler *);
        void push(int, Packet* );
        char buffer[10000010];
        int flag[100010];
        int cnt = 0;
	int tail = 0;	
    private:
        uint8_t type;
};

CLICK_ENDDECLS
#endif
