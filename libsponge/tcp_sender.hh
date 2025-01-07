#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <map>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    // 重传计数器超时时间 RTO 
    int _timeout{-1};

    // 重传计数器 -- 记录当前距离重传计时器启动已经过了多久或者距离上一个package被重传过了多久
    int _timecount{0};

    // 记录已经发送但是还没有确认的TCP报文段及其起始序列号---该集合是有序的
    std::map<size_t, TCPSegment> _outgoing_map{};

    // 记录已经发送但是还没有确认的字节数量
    size_t _outgoing_bytes{0};

    // 记录接收端上一次传回来的窗口大小
    size_t _last_window_size{1};

    // 是否设置SYN标志
    bool _set_syn_flag{false};

    // 是否设置FIN标志
    bool _set_fin_flag{false};

    // 连续重传计数
    size_t _consecutive_retransmissions_count{0};

    //! our initial sequence number, the number for our SYN.
    // 初始序列号
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    // 将需要发送的TCP数据报塞入这个队列即可发送出去
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    // 重传计时器初始的重传时间
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    // 等待被发送的字节流
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    // 下一个发送的字节对应的序列号
    uint64_t _next_seqno{0};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    // 成员函数
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
