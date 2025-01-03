#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return 0;
}

size_t TCPConnection::bytes_in_flight() const { 
    return 0;
}

size_t TCPConnection::unassembled_bytes() const { 
    return 0;
}

size_t TCPConnection::time_since_last_segment_received() const { 
    return 0;
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    DUMMY_CODE(seg);
}

bool TCPConnection::active() const { 
    return false;
}

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data); 
    return 0;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    DUMMY_CODE(ms_since_last_tick);
}

void TCPConnection::end_input_stream() {}

void TCPConnection::connect() {}

TCPConnection::~TCPConnection() {}

void TCPConnection::_set_rst_state(bool send_rst) {
    DUMMY_CODE(send_rst);
}

void TCPConnection::_trans_segments_to_out_with_ack_and_win() {}