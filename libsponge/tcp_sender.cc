#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

// 返回已经发送但是没有手动ack的字节数量
uint64_t TCPSender::bytes_in_flight() const {
    return _outgoing_bytes;
}

void TCPSender::fill_window() {
    // 初始化当前窗口大小
    size_t curr_window_size = _last_window_size ? _last_window_size : 1;

    // 循环填充窗口
    /*
        1、每次发送数据时，都会尝试构建一个新的 TCP 数据包 segment
        2、如果 SYN 标志尚未被设置（即这是连接的第一次发送），就设置 SYN 标志，并标记 _set_syn_flag = true，表示 SYN 已经被发送。
        3、设置序列号
        4、计算payload长度，并且设置payload数据部分
        5、设置 FIN 标志
        6、将数据部分添加到segment对应部分(payload)
        7、检查是否有有效数据
        8、重传计时器管理
        9、发送数据包
        10、追踪已发送的字节
        11、检查是否发送了 FIN 数据包
    */
    while (curr_window_size > _outgoing_bytes) {
        TCPSegment segment;
        if (!_set_syn_flag) {
            segment.header().syn = true;
            _set_syn_flag = true;
        }

        // 设置序列号, 下一批数据的起始序列号
        segment.header().seqno = next_seqno();

        // 计算并且设置数据部分(payload), 取配置文件中payload的值和当前窗口 - 已发出还未确认的数据 - syn 所占用的序列号, 二者较小的一个
        const size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, curr_window_size - _outgoing_bytes - segment.header().syn);
        string payload = _stream.read(payload_size);

        // 设置 FIN 标志位， 如果尚未发送 FIN 并且字节流已经结束（_stream.eof()）并且当前窗口空间足够容纳一个 FIN 标志
        if (!_set_fin_flag && _stream.eof() && payload.size() + _outgoing_bytes < curr_window_size)
            _set_fin_flag = segment.header().fin = true;

        // step 6, 直接转移payload，不用copy节省开销
        segment.payload() = Buffer(move(payload));

        // step 7, 如果没有数据在序列号空间中(包括没有 syn 和 fin)，直接退出
        if (segment.length_in_sequence_space() == 0) { break; }
        
        // step 8, 如果没有待重传的数据包, 即 _outgoing_map 为空, 设置初始的重传超时时间 _timeout 和计时器的计数器 _timecount
        if (_outgoing_map.empty()) {
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }

        // step 9, 将组装好的segment塞入_segments_out队列中
        _segments_out.push(segment);

        // step 10, a、更新已发未确认的字节数量 b、记录已发未确认的数据 c、更新_next_seqno
        _outgoing_bytes += segment.length_in_sequence_space();
        _outgoing_map.insert(make_pair(_next_seqno, segment));
        _next_seqno += segment.length_in_sequence_space();

        // step 11
        if (segment.header().fin) { break; }
    }
    
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_seqno = unwrap(ackno, _isn, _next_seqno);
    // 如果传入的 ack 是不可靠的，则直接丢弃
    if (abs_seqno > _next_seqno)
        return;
    // 遍历数据结构，将已经接收到的数据包丢弃
    for (auto iter = _outgoing_map.begin(); iter != _outgoing_map.end();) {
        // 如果一个发送的数据包已经被成功接收
        const TCPSegment &seg = iter->second;
        // 当前数据包的起始序列号加上总字节数 <= 接收端传回的ackno,说明当前数据包已经被成功接收了
        if (iter->first + seg.length_in_sequence_space() <= abs_seqno) {
            // 已经发出但是还未确认的字节数减去对应的大小
            _outgoing_bytes -= seg.length_in_sequence_space();
            // 从map集合中移除当前数据包
            iter = _outgoing_map.erase(iter);

            // 如果有新的数据包被成功接收，则清空超时时间
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }
        // 如果当前遍历到的数据包还没被接收，则说明后面的数据包均未被接收，因此直接返回
        else
            break;
    }
    // 重传次数归零
    _consecutive_retransmissions_count = 0;
    // 更新当前接收方窗口大小
    _last_window_size = window_size;
    // 尝试传输数据
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 重传计数器累加计时 
    _timecount += ms_since_last_tick;

    auto iter = _outgoing_map.begin();
    // 如果存在发送中的数据包，并且定时器超时
    if (iter != _outgoing_map.end() && _timecount >= _timeout) {
        // 如果窗口大小不为0还超时，则说明网络拥堵 --- 超时时间翻倍
        if (_last_window_size > 0)
            _timeout *= 2;
        // 重传计数器清零,因为下面要进行重传操作    
        _timecount = 0;
        // 重传最早还未确认的数据包
        _segments_out.push(iter->second);
        // 连续重传计时器增加
        ++_consecutive_retransmissions_count;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}