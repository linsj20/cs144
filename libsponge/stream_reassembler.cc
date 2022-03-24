#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity): _sstream(), _stream(), _head(0), _tail(0xffffffffffffffff),
                                                             _output(capacity), _capacity(capacity){
}

string StreamReassembler::string_chunk(const string &data,size_t o_index,size_t& n_index){
  size_t s_end = o_index + data.length();
  if(o_index < _head) {
    if(s_end > _head) {
      n_index = _head;
      size_t max_s = _head + _capacity - _output.buffer_size();
      if(s_end > max_s) {
        return data.substr(n_index - o_index , max_s - n_index);
      }
      else {return data.substr(n_index - o_index);}
    }
    else {return "";}
  }
  else {
    size_t max_s = _head + _capacity - _output.buffer_size();
    if(o_index < max_s){
      n_index = o_index;
      if(s_end > max_s) {return data.substr(0 , max_s - n_index);}
      else {return data;}
    }
    else {return "";}
  }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {

  if(eof){_tail = index + data.length();}
  if(data.empty()) {
    if(_head >= _tail){_output.end_input();}
    return;
  }
  size_t n_index;
  string n_data = string_chunk(data ,index ,n_index);
  if(n_data.empty()) {
    if(_head >= _tail){_output.end_input();}
    return;
  }
  size_t n_end = n_index + n_data.length();
  if(_sstream.empty()){
    if(n_index == _head) {
      _output.write(n_data);
      _head = n_end;
      if(_head >= _tail){_output.end_input();}
      return;}
    else {
      _sstream[n_index] = n_data;
      _stream[n_index] = true;
      _stream[n_end] = false;
      if(_head >= _tail){_output.end_input();}
      return;
    }
  }

  auto s_itr = _stream.lower_bound(n_index);
  auto e_itr = _stream.lower_bound(n_end);
  bool state_l = false,state_r = false;

  if(s_itr == _stream.end()){
    state_l = true;
  }
  else {
    if(s_itr->first == n_index){
      if(!s_itr->second){
        s_itr--;
        n_data = _sstream[s_itr->first] + n_data;
        n_index = s_itr->first;
      }
    }
    else{
      if(!s_itr->second){
        s_itr--;
        n_data = _sstream[s_itr->first].substr(0,n_index - s_itr->first) + n_data;
        n_index = s_itr->first;
      }
      state_l = true;
    }
  }

  if(e_itr == _stream.end()){
    state_r = true;
  }
  else{
    if(e_itr->first == n_end){
      if(e_itr->second){
        n_data = n_data + _sstream[e_itr->first];
        n_end += _sstream[e_itr->first].length();
      }
    }
    else{
      if(!e_itr->second){
        e_itr--;
        string temp = _sstream[e_itr->first].substr(n_end - e_itr->first);
        n_data = n_data + temp;
        n_end += temp.length();
      }
      state_r = true;
    }
  }
  if(state_l)_stream[n_index] = true;
  if(state_r)_stream[n_end] = false;
  _sstream[n_index] = n_data;

  s_itr = _stream.lower_bound(n_index+1);
  e_itr = _stream.find(n_end);
  auto ss_itr = _sstream.lower_bound(n_index + 1);
  auto ee_itr = _sstream.lower_bound(n_end);
  _stream.erase(s_itr,e_itr);
  _sstream.erase(ss_itr,ee_itr);

  if(_stream.find(_head) != _stream.end()){
    _output.write(_sstream[_head]);
    _head += _sstream[_head].length();
    auto itr = _stream.begin();
    _sstream.erase(itr->first);
    itr++;itr++;
    _stream.erase(_stream.begin(),itr);
  }
  if(_head >= _tail){_output.end_input();}
}

size_t StreamReassembler::unassembled_bytes() const {
  auto itr = _sstream.begin();
  size_t sum = 0;
  while(itr != _sstream.end()){
    sum += itr->second.length();
    itr++;
  }
  return sum;
}

bool StreamReassembler::empty() const { return _sstream.empty(); }

