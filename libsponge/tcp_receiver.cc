#include "tcp_receiver.hh"

#include <cassert>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

/**
 *  \brief 当前 TCPReceiver 大体上有三种状态， 分别是
 *      1. LISTEN，此时 SYN 包尚未抵达。可以通过 _set_syn_flag 标志位来判断是否在当前状态
 *      2. SYN_RECV, 此时 SYN 抵达。只能判断当前不在 1、3状态时才能确定在当前状态
 *      3. FIN_RECV, 此时 FIN 抵达。可以通过 ByteStream end_input 来判断是否在当前状态
 */

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // check syn
    // 如果 tcp 头部的syn被设置，记录isn_初始序列号
    if (seg.header().syn) {
        isn_ = seg.header().seqno;
    }
    // 如果isn_没有值，说明还没有建立连接，直接返回
    if (!isn_.has_value()) return;

    // check fin
    // 如果 fin 被设置，记录fin_seq_结束序列号
    if (seg.header().fin) {
        fin_seq_ = unwrap(seg.header().seqno, isn_.value(), seqno_) + seg.length_in_sequence_space();
    }
    
    // 将当前 32 位 转换位 64 位绝对序列号
    uint64_t index = unwrap(seg.header().seqno, isn_.value(), seqno_);
    
    // 如果 syn 被设置, 减去 syn 占用的序列号
    if (!seg.header().syn) index--;

    // 将 seg 的数据部分写入重组器
    reassembler_.push_substring(seg.payload().copy(), index, seg.header().fin);

    // 更新期望的序列号seqno_
    seqno_ = reassembler_.stream_out().bytes_written() + 1;

    // 如果 fin 被设置，seqno_还需要再加 1, fin_seq 也占一个 seqno_
    if (fin_seq_.has_value() && fin_seq_.value() == seqno_ + 1) seqno_++;

}

// 如果连接建立，ack就是seqno_
optional<WrappingInt32> TCPReceiver::ackno() const {
    return isn_.has_value() ? wrap(seqno_, isn_.value()) : isn_;
}

// 当前滑动窗口的大小，就是总容量 - 已写 - 已读
size_t TCPReceiver::window_size() const { 
    return capacity_ - (reassembler_.stream_out().bytes_written() - reassembler_.stream_out().bytes_read());
}
