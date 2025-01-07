#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

// 发送窗口剩余空闲空间, 从tcpsender中的stream_in()函数中可以获取到ByteStream类
size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

// 已发送但还没有ack的字节数量, 调用tcpsender的bytes_in_flight函数,获取_outgoing_bytes
size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

// 未按序到达的字节数量, 调用tcpreceiver中的unassembled_bytes函数
size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

// 最后一次收包时间
size_t TCPConnection::time_since_last_segment_received() const { 
    return _time_since_last_segment_received_ms;
}

// tcp连接是否还存活
bool TCPConnection::active() const { 
    return _is_active;
}

// 将待发送的数据包加上期望接受到数据的ackno和当前自己作为接收端的滑动窗口大小
// 将 TCPSender 中待发送的数据包（segments_out()）加入到 TCPConnection 的发送队列中，并在必要时设置 TCP 头部的 ACK 和窗口大小（win）
void TCPConnection::_trans_segments_to_out_with_ack_and_win() {
    /*
        1、循环处理待发送的数据包
        2、检查是否已建立连接
        3、设置ACK和窗口大小
        4、将数据包添加到segments_out队列中
    */
    while (!_sender.segments_out().empty()) {
        // step 1
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // step 2, 如果已经建立连接从tcpreceiver中获取数据，设置ack标志位;设置Ack序列号;设置窗口大小
        if (_receiver.ackno().has_value()) {
            // step 3
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        // step 4
        _segments_out.push(seg);
    }
}

// 关闭tcpsender写通道
void TCPConnection::end_input_stream() {
    // 关闭发送端的写入流通道 -- 此时不能写,但是可以将写入缓冲区中剩余数据全部读取完毕
    _sender.stream_in().end_input();
    // 在输入流结束后，必须立即发送 FIN -- 如果写入缓冲区还存在数据,会先发送完毕,最后发送FIN
    _sender.fill_window();
    // 将等待发送的数据包加上本地的 ackno 和 window size
    _trans_segments_to_out_with_ack_and_win();
}

// 建立连接
void TCPConnection::connect() {
    // 第一次调用fill_winodw()会发送一个syn数据包
    _sender.fill_window();
    // 设置存活标志位
    _is_active = true;
    // 携带本地的ackno 和 window size
    _trans_segments_to_out_with_ack_and_win();
}

// TCPConnection的析构函数，析构函数通常用于清理对象销毁时的资源，确保在对象生命周期结束时进行必要的清理操作
TCPConnection::~TCPConnection() {
    try {
        if (active()){
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _set_rst_state(false);
        }
    } catch(const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// 接受一个布尔参数 send_rst，决定是否发送 RST 包来强制终止连接或清除连接的异常状态
void TCPConnection::_set_rst_state(bool send_rst) {
    /*
        1、构造一个rst包，添加到数据报队列中
        2、关闭输入输出流
        3、更新标志，更新存活信号
    */

    if (send_rst) {
        // 发送一个RST包,通知对端接受者立即终止本次TCP连接
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
    }

    // step 2, 如果不标记为错误状态，连接的其他部分可能仍然尝试向这些流中写入数据或读取数据，而这些数据可能无法正确处理或丢失
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();

    // step 3, 连接已经彻底断开,所以不需要再等待2MSL秒了
    _linger_after_streams_finish = false;
    _is_active = false;
}

// 发送数据
size_t TCPConnection::write(const string &data) {
    /*
        1、向缓冲区中写入数据
        2、根据对端缓冲区大小，控制本地的窗口大小
        3、将本地等待的数据包加上 ackno 和 window_size
    */
   size_t write_size = _sender.stream_in().write(data);
   _sender.fill_window();
   _trans_segments_to_out_with_ack_and_win();
   return write_size;
}

// 接收到一个tcp数据段时候的处理逻辑
void TCPConnection::segment_received(const TCPSegment &seg) { 
    /*
        1、更新时间戳
        2、确定是否需要发送ack
        3、接收数据传递给_receive接收器
        4、处置rst包
        5、确保发送队列为空
        6、处理接收到的ack包
        7、从 LISTEN 状态转到 SYN_SENT 状态
        8、处理 TCP 断开连接时的状态
        9、处理连接关闭（CLOSE_WAIT 状态）
        10、处理空数据包的 ACK
        11、发送包含 ACK 和窗口大小的段
    */
    _time_since_last_segment_received_ms = 0;
    bool need_send_ackno =  seg.length_in_sequence_space();
    _receiver.segment_received(seg);
    if (seg.header().rst) {
        _set_rst_state(false);
        return;
    }
    // 确保在处理接收到的TCP段之前，发送器没有待发送的TCP段
    assert(_sender.segments_out().empty());
    
    // 如果收到了 ACK 包，则更新 _sender 的状态并补充发送数据
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        // 如果有ack包并且队列中不为空，更新need_send_ackno标志
        if (seg.header().ack && !_sender.segments_out().empty())
            need_send_ackno = false;
    }

    // 如果第一次收到syn数据包，并且sender状态还是关闭状态，将状态由Listen跟新到syn_sent
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED){
            connect();
            return;
        }

    // 判断断开连接是否需要等待
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
            _linger_after_streams_finish = false;
        }

    // 准备断开连接时，服务器端先断开连接, _linger_after_streams_finish为false确保服务器先断开
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && 
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
            _is_active = false;
            return;
        }
    
    // 如果收到的数据包中没有任何数据，该数据包是keep-alive
    if (need_send_ackno) {
        _sender.send_empty_segment();
    }

    _trans_segments_to_out_with_ack_and_win();
}


//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    /*
        1、确保发送队列为空
        2、调用_sender.tick()更新发送器状态
        3、检测重传次数是否超过最大值
        4、转发待重传数据段
        5、更新接收时间计数器
        6、判断 time_wait 是否超时
    */

    assert(_sender.segments_out().empty());
    _sender.tick(ms_since_last_tick);
    // 如果重传次数大于默认最大尝试次数，清理当前重传队列，发送rst包，返回
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _sender.segments_out().pop();
        _set_rst_state(true);
        return;
    }
    _trans_segments_to_out_with_ack_and_win();
    _time_since_last_segment_received_ms += ms_since_last_tick;
    // _linger_after_streams_finish 确保是客户端的行为
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        _linger_after_streams_finish && _time_since_last_segment_received_ms >= 10 * _cfg.rt_timeout) {
            _is_active = false;
            _linger_after_streams_finish = false;
        }

}

