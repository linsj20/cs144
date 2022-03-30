#include "tcp_sender.hh"

#include "tcp_helpers/tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
  _retransmission_timeout = _initial_retransmission_timeout;
  _retransmission_seq = 0;
  _consecutive_retransmissions = 0;
  _last_ack = _isn.raw_value();
  _time = 0;
  _window = 1;
}

uint64_t TCPSender::bytes_in_flight() const {
  uint64_t res = 0;
  auto itr = _outstanding.begin();
  while(itr != _outstanding.end()){
    res += itr->second.second.length_in_sequence_space();  //ack / fin?
    itr++;
  }
  return res;
}

void TCPSender::fill_window() {
  if(!_syn){
      _syn = true;
      TCPSegment seg;
      seg.header().syn = true;
      seg.header().seqno = _isn;
      _next_seqno = seg.length_in_sequence_space();
      _outstanding[_next_seqno] = make_pair(_time, seg);
      _segments_out.push(seg);
      return;
  }

  uint32_t window_remained = unwrap(WrappingInt32(_last_ack + (_window > 0 ? _window : 1)), _isn, _next_seqno) -
      _next_seqno;
  if(_stream.eof() && !closed && window_remained > _stream.buffer_size()){
    TCPSegment seg;
    seg.header().fin = true;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    _next_seqno += seg.length_in_sequence_space();
    _outstanding[_next_seqno] = make_pair(_time, seg);
    window_remained -= seg.length_in_sequence_space();
    closed = true;
  }
  while (window_remained > 0){
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    uint32_t to_be_sent = TCPConfig::MAX_PAYLOAD_SIZE > window_remained ? window_remained : TCPConfig::MAX_PAYLOAD_SIZE;
    to_be_sent = to_be_sent > _stream.buffer_size() ? _stream.buffer_size() : to_be_sent;
    if(to_be_sent == 0){break;}
    seg.payload() = Buffer(_stream.read(to_be_sent));
    if(_stream.eof() && window_remained > to_be_sent){seg.header().fin = true;closed = true;}
    _next_seqno += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _outstanding[_next_seqno] = make_pair(_time, seg);
    window_remained -= seg.length_in_sequence_space();
  }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  _window = window_size;
  size_t received_seq =  unwrap(ackno, _isn, _next_seqno);
  _last_ack = unwrap(WrappingInt32(_last_ack), _isn, _next_seqno) >= received_seq ?
                                                                                  _last_ack : ackno.raw_value();
  auto itr = _outstanding.lower_bound(received_seq);
  if(itr == _outstanding.end() || itr->first != received_seq){fill_window();return;}
  itr++;
  auto itr3 = itr;
  while(itr3 != _outstanding.end()){itr3->second.first = _time;itr3++;}
  _outstanding.erase(_outstanding.begin(),itr);
  if(_consecutive_retransmissions > 0 && unwrap(ackno,_isn,_next_seqno) >= _retransmission_seq){
    _consecutive_retransmissions = 0,_retransmission_timeout = _initial_retransmission_timeout;
  }
  fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {//might induce confusion
  _time += ms_since_last_tick;
  if(_outstanding.empty())return;
  auto tg = _outstanding.begin()->second;
  if(_time - tg.first >= _retransmission_timeout){
    _segments_out.push(tg.second);
    if(_consecutive_retransmissions == 0){_retransmission_seq = tg.first;}
    _outstanding.begin()->second = make_pair(_time, tg.second);
    _consecutive_retransmissions++;
    if(_window != 0){_retransmission_timeout *= 2;}
  }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
  TCPSegment seg;
  seg.header().seqno = wrap(_next_seqno, _isn);
  _segments_out.push(seg);
}

