#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _usage(0), _writes(0), _reads(0), _buffer(""), _end_input(false) {}

size_t ByteStream::write(const string &data) {
    if (data.length() + _usage <= _capacity) {
        _buffer += data;
        _usage += data.length();
        _writes += data.length();
        return data.length();
    } else {
        size_t writes = _capacity - _usage;
        if (!writes)
            return 0;
        _buffer += data.substr(0, _capacity - _usage);
        _usage = _capacity;
        _writes += writes;
        return writes;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (_usage >= len)
        return _buffer.substr(0, len);
    else
        return _buffer;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (_usage >= len) {
        _buffer = _buffer.substr(len, _buffer.length());
        _usage -= len;
        _reads += len;
    } else {
        _buffer = "";
        _usage = 0;
        _reads += _buffer.length();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (_usage >= len) {
        _usage -= len;
        _reads += len;
        std::string temp = _buffer.substr(0, len);
        _buffer = _buffer.substr(len, _buffer.length());
        return temp;
    } else {
        _usage = 0;
        _reads += _buffer.length();
        std::string temp = _buffer;
        _buffer = "";
        return temp;
    }
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _usage; }

bool ByteStream::buffer_empty() const { return !_usage; }

bool ByteStream::eof() const { return (!_usage) && (_end_input); }

size_t ByteStream::bytes_written() const { return _writes; }

size_t ByteStream::bytes_read() const { return _reads; }

size_t ByteStream::remaining_capacity() const { return _capacity - _usage; }

