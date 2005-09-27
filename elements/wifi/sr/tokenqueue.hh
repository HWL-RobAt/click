// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOKENQUEUE_HH
#define CLICK_TOKENQUEUE_HH
#include <elements/standard/notifierqueue.hh>
#include <click/bighashmap.hh>
#include <elements/wifi/sr/path.hh>
#include <elements/wifi/sr/srpacket.hh>
CLICK_DECLS

/*
=c

TokenQueue
TokenQueue(CAPACITY)


=s storage

inputs:
0 - normal
1 - forwarding packets - spit out on output 1
2 - retransmits
stores packets in a queue

=a Queue, SimpleQueue, FrontDropQueue */

class TokenQueue : public NotifierQueue { public:
    
    TokenQueue();
    ~TokenQueue();
    
    const char *class_name() const	{ return "TokenQueue"; }
    void *cast(const char *);
    const char *port_count() const	{ return "3/2"; }
    const char *processing() const	{ return "hhh/lh"; }
    inline bool enq(Packet *p);
    Packet *pull(int);
    void push(int port, Packet *);
    void process_source(struct srpacket *);
    void process_forward(struct srpacket *);
    int configure(Vector<String> &, ErrorHandler *);    
    void run_timer(Timer *);
private:
    int _drops;
    Timestamp _catchup_timeout;

    class PathInfo {
    public:
	TokenQueue *_q;
	Path _p;
	unsigned int _seq;
	Timestamp _last_tx;
	Timestamp _first_tx;
	Timestamp _first_rx;
	Timestamp _last_rx;
	Timestamp _last_real;
	int _packets_tx;
	int _packets_rx;
	int _expected_rx;
	int _tokens_passed;
	bool _congestion;
	bool _token;    // i can send now
	bool _rx_token; //got a packet with token in it
	bool _active;
	IPAddress _towards;
	void reset() {
	    _token = false;
	    _rx_token = false;
	    _seq = 0; 
	    _congestion = false; 
	    _packets_tx = 0;
	    _packets_rx = 0;
	    _tokens_passed = 0;
	}
	void reset_rx(unsigned int seq, IPAddress towards) {
	    _token = false;
	    _rx_token = false;
	    _seq = seq;
	    _packets_rx = 0;
	    _towards = towards;
	}
	PathInfo() :  _p() { reset(); }
	PathInfo(Path p, TokenQueue *q) :  _q(q), _p(p) { reset(); }

	bool is_endpoint(IPAddress ip) const{
	    if (_p.size() < 1) {
		return false;
	    }
	    return (_p[0] == ip || _p[_p.size()-1] == ip);
	}
	
	IPAddress other_endpoint(IPAddress ip) {
	    return (ip == _p[0]) ? _p[_p.size()-1] : _p[0];
	}

	bool rt_timedout() const {
	    if (!_q) {
		return false;
	    }
	    /* round trip timeout is
	     * packets expected * max_tx_time * route_len 
	     */
	    unsigned int rt_duration_ms = 
		(_q->_threshold * _q->_max_tx_packet_ms * (_p.size()+1));
	    
	    Timestamp expire = _last_tx + Timestamp::make_msec(rt_duration_ms);
	    return expire < Timestamp::now();
	}

	bool active_timedout() const {
	    if (!_q) {
		return false;
	    }
	    Timestamp expire = _last_real + _q->_active_duration;
	    return expire < Timestamp::now();
	}

	bool clear_timedout() const {
	    if (!_q || _active) {
		return false;
	    }
	    Timestamp expire = _last_real + _q->_clear_duration;
	    return expire < Timestamp::now();
	}

    };
    friend class PathInfo;

    typedef HashMap<Path, PathInfo> PathTable;
    typedef PathTable::const_iterator PathIter;
    PathTable _paths;
    

    struct yank_filter {
	TokenQueue *s;
	Path _p;
	yank_filter(TokenQueue *t, Path p) {
	    s = t;
	    _p = p;
	}
	bool operator()(const Packet *p) {
	    return (s) ? s->ready_for(p, _p) : false;
	}
    };
    friend class yank_filter;

    PathInfo *find_path_info(Path);
    bool ready_for(const Packet *, Path);
    int bubble_up(Packet *);
    String print_stats();
    static String static_print_stats(Element *, void *);
    void add_handlers();
    static int static_clear(const String &arg, Element *e,
			    void *, ErrorHandler *errh); 
    void clear();
    static int static_write_debug(const String &arg, Element *e,
				  void *, ErrorHandler *errh); 
    
    static int static_write_packet_timeout(const String &arg, Element *e,
					   void *, ErrorHandler *errh); 
    static int static_write_threshold(const String &arg, Element *e,
				      void *, ErrorHandler *errh); 
    int set_packet_timeout(ErrorHandler *, unsigned int);
    int set_threshold(ErrorHandler *, int);

    static String static_print_debug(Element *, void *);
    static String static_print_packet_timeout(Element *, void *);
    static String static_print_threshold(Element *, void *);
    
    uint16_t _et;     // This protocol's ethertype
    class SRForwarder *_sr_forwarder;
    bool _debug;
    int _threshold;
    unsigned int _max_tx_packet_ms;
    Timestamp _active_duration;
    Timestamp _clear_duration;
    Timer _timer;
    int _tokens;
    int _retransmits;
    int _normal;
};

inline bool
TokenQueue::enq(Packet *p)
{
    sr_assert(p);
    int next = next_i(_tail);
    if (next != _head) {
	_q[_tail] = p;
	_tail = next;
	return true;
    } else {
	p->kill();
	if (_drops == 0) {
	    click_chatter("%{element}: overflow", this);
	}
	_drops++;
    }

    return false;
}

CLICK_ENDDECLS
#endif
