#ifndef TCP_HH
#define TCP_HH

#include <click/element.hh>
#include <click/timer.hh>
#include "packets.hh"
#include <queue>

#define SYN_SET (1<<1)
#define ACK_SET (1<<4)
#define FIN_SET (1<<0)
#define    FIN_SENT 4
#define    FIN_WAIT 5
#define    CLOSED 0
#define    SYN_SENT 1
#define    SYN_WAIT 2
#define    CONNECTED 3
#define    MAX_ON_WIRE 1024
#define    CLIENT_BUFFER_SIZE 256
using std::queue;
CLICK_DECLS

class TCPElement : public Element {
public:
    TCPElement();

    ~TCPElement();

    const char *class_name() const { return "tcpEntity"; }

    const char *port_count() const { return "2-/2-"; }

    const char *processing() const { return PUSH; }

    int configure(Vector <String> &conf, ErrorHandler *errh);

    void push(int port, Packet *packet);

    int initialize(ErrorHandler *);

    void set_timer(Packet *p,uint16_t);

    void set_fin_timer();

    void set_client_timer();

    void run_timer(Timer *timer);

    void cancel_timer();

    void cancel_fin_timer();

    void cancel_client_timer();

private:
    Timer timer_resend;
    Timer timer_debug;
    Timer timer_fin;
    Packet *info_resend;
    Packet *info_fin;
    Packet* slot[CLIENT_BUFFER_SIZE];
    Timer client_submit_timer;
    uint32_t myip;
    uint16_t myport;
    uint32_t coip;
    uint16_t coport;
    uint32_t timeout;
    uint32_t nxt_ack;
    uint32_t nxt_seq;
    uint32_t state;
    uint32_t cur_data;
    uint32_t base_data;
    uint32_t cwnd;
    uint32_t lost;
    uint32_t acc_succeed;
    uint32_t last_ack;
    uint32_t acked_max;

    void makeTCPhead(struct MyTCPHeader *format, uint8_t flags) {
        format->type = flags;
        format->size = sizeof(struct MyTCPHeader);
        format->src = myip;
        format->src_port = myport;
        format->dest = coip;
	format->slot = CLIENT_BUFFER_SIZE;
        format->dest_port = coport;
        format->ack = nxt_ack;
        format->seq = nxt_seq;
        format->count = 0;
	format->acked_max = acked_max;
    }
    queue<Packet *> server_buffer;
    queue<Packet *> buffer;
    
    uint8_t tcpele_type;            //server or client
};


CLICK_ENDDECLS
#endif 


