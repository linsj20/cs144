#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timeout; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _timeout = 0;
    if(seg.header().rst){
        inbound_stream().set_error();
        _sender.stream_in().set_error();
        _sender.stream_in().set_error(),_receiver.stream_out().set_error();
    }
    bool any_send = false;
    if(seg.length_in_sequence_space() == 0){any_send = true;}
    _receiver.segment_received(seg);
    if(seg.header().ack) {
        auto before = _sender.next_seqno();
        _sender.ack_received(seg.header().ackno, seg.header().win);
        auto after = _sender.next_seqno();
        if(before != after){any_send = true;}
    }
    if(!any_send){
        TCPSegment seg0;
        seg0.header().ack = true;
        seg0.header().ackno = _receiver.determined_ack();
        seg0.header().win = _receiver.window_size();
        _segments_out.push(seg0);
    }
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        _segments_out.back().header().win = _receiver.window_size();
        if(_receiver.ackno()){_segments_out.back().header().ack = true;}
    }
}

bool TCPConnection::active() const {
    if(_sender.stream_in().error() || _receiver.stream_out().error()){return false;}
    if(_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.bytes_in_flight() == 0){
        return _linger_after_streams_finish;
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t writes = _sender.stream_in().write(data);
    _sender.fill_window();
    return writes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _timeout+= ms_since_last_tick;
    if(_timeout >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
    }
    //_sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS){
        _sender.send_empty_segment();
        segments_out().back().header().rst = true;
        _sender.stream_in().set_error(),_receiver.stream_out().set_error();
    }
}

void TCPConnection::end_input_stream() {
    TCPSegment seg;
    seg.header().fin = true;
    seg.header().ack = true;
    seg.header().ackno = _receiver.determined_ack();
    seg.header().seqno = wrap(_seqno,WrappingInt32(0));
    _segments_out.push(seg);
}

void TCPConnection::connect() {     //TBD
        TCPSegment seg;
        seg.header().syn = true;
        _segments_out.push(seg);
        _seqno += seg.length_in_sequence_space();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            segments_out().back().header().rst = true;
            _sender.stream_in().set_error(),_receiver.stream_out().set_error();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
