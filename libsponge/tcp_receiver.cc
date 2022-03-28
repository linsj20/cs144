#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader tcp_h = seg.header();
    if(!_syn) {
      if(tcp_h.syn){
        _syn = true;
        _fin = false;
        _isn = tcp_h.seqno.raw_value();
        _reassembler.push_substring(string(seg.payload().str()),
                                    unwrap(tcp_h.seqno ,WrappingInt32(_isn), 0),
                                    tcp_h.fin);
        _checkpoint = seg.length_in_sequence_space() - 1;
        if(tcp_h.fin){
          _fin = true;
        }
        return;
      }
      else {return;}
    }
    if(string(seg.payload().str()) != "") {
      _reassembler.push_substring(string(seg.payload().str()),
                                  unwrap(tcp_h.seqno,WrappingInt32(_isn),_checkpoint)-1,
                                  tcp_h.fin);
      if (_checkpoint + 1 == unwrap(tcp_h.seqno, WrappingInt32(_isn), _checkpoint)) {
        _checkpoint = _reassembler.head();
      }
    }

    if( tcp_h.fin){_fin = true;}
    if(_fin){
      if(_reassembler.empty()){
        _checkpoint = _reassembler.stream_out().eof() ? tcp_h.ackno.raw_value() - 1 : _checkpoint + 1;
        _reassembler.stream_out().end_input();
      }
    }
}
WrappingInt32 TCPReceiver::determined_ack() const {
    return wrap(_checkpoint + 1, WrappingInt32(_isn));
}
std::optional<WrappingInt32> TCPReceiver::ackno() const {
  if(!_syn){return {};}
  else {return determined_ack();}
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

