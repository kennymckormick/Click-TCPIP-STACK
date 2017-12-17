#ifndef SERVER_HH
#define SERVER_HH
#include <click/element.hh>

#define MESSAGE 0
#define DATA 1

CLICK_DECLS

class Server: public Element {
    public: 
        const char *class_name() const { return "Server";}
        const char *port_count() const { return "0/1"; }
        const char *process() const { return PUSH; }
        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler*);
	private:
	    String str;
        uint8_t type;
        uint32_t dst_ip;
        uint32_t dst_port;
};

CLICK_ENDDECLS
#endif
