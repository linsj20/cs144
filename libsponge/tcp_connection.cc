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
    if(!active()){return;}
    _timeout = 0;

    if (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0) {
        if (!seg.header().syn)
            return;
        _receiver.segment_received(seg);
        connect();
        return;
    }

    if (_sender.next_seqno_absolute() > 0 && _sender.bytes_in_flight() == _sender.next_seqno_absolute() &&
        !_receiver.ackno().has_value()) {
        if (seg.payload().size())
            return;
        if (!seg.header().ack) {
            if (seg.header().syn) {
                // simultaneous open
                _receiver.segment_received(seg);
                _sender.send_empty_segment();
            }
            return;
        }
        if (seg.header().rst) {
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _active = false;
            return;
        }
    }
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);

    if (_sender.stream_in().buffer_empty() && seg.length_in_sequence_space() > 0)
        _sender.send_empty_segment();
    if (seg.header().rst) {
        _sender.send_empty_segment();
        unclean_shutdown();
        return;
    }
    send_sender_segments();
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string &data) {
    if(data.empty()){return 0;}
    size_t writes = _sender.stream_in().write(data);
    _sender.fill_window();
    send_sender_segments();
    return writes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active){return;}
    _timeout += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){unclean_shutdown();}
    send_sender_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_sender_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_sender_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::send_sender_segments() {
    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    clean_shutdown();
}

void TCPConnection::unclean_shutdown() {
    // When this being called, _sender.stream_out() should not be empty.
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    seg.header().ack = true;
    if (_receiver.ackno().has_value())
        seg.header().ackno = _receiver.ackno().value();
    seg.header().win = _receiver.window_size();
    seg.header().rst = true;
    _segments_out.push(seg);
}

void TCPConnection::clean_shutdown() {
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof())
            _linger_after_streams_finish = false;
        else if (_sender.bytes_in_flight() == 0) {
            if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                _active = false;
            }
        }
    }
}
