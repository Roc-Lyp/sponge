#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 将64位绝对序列号转换为32位序列号 (包裹)
/*
    1、n 是输入的64位绝对序列号
    2、isn 是 初始序列号（ISN），它是 32 位的，通常用于表示一个 TCP 连接的起始位置

步骤
    1、将 n 转换为 32 位整数 n 的低 32 位部分。因为输入的 n 是 64 位的，所以我们通过将 n 转为 32 位数来丢弃高 32 位
    2、将 isn 和 低32位的 n 相加
*/
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32{isn + static_cast<uint32_t>(n)};
}


//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
// 将TCP协议头中携带的32位序列号转换为64位绝对序列号（解包）
// 参数: 要转换的32位序列号,本次TCP连接的ISN(初始序列号),检查点(一个32位序列号对应多个64位序列号,因此这里选择靠近ISN的值)
/*
    1、checkpoint - offset：这计算的是从 offset 到 checkpoint 之间的差值。
        由于 offset 是相对的 32 位序列号，而 checkpoint 是 64 位绝对序列号，这个差值可能会跨越 2^32 的边界。
    2、(checkpoint - offset) >> 32：这是一个右移操作，将 checkpoint - offset 的高 32 位取出。
        通过右移 32 位，我们得到了差值的高 32 位部分。这一部分代表了 offset 和 checkpoint 之间跨越了多少个完整的 2^32 区间。
    3、* (1lu << 32)：这里的 1lu << 32 是 2^32，即一个 32 位整数的完整区间。
        乘上 2^32，是将差值中的高 32 位还原回实际的序列号差。通过这个操作，我们就能得到跨越了多少个 2^32 的倍数。
    4、offset +=：最后将这个调整量加到原来的 offset 上，确保它被正确调整到相对的绝对序列号。

    checkpoint - offset <= (1lu << 31)：这里检查 checkpoint - offset 是否小于等于 2^31，即差值是否小于一个 32 位整数的一半（2^31）
    如果差值小于等于 2^31，这意味着 checkpoint 和 offset 之间的差距在 32 位序列号范围内（不跨越 2^32），所以我们可以直接返回 offset
*/
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // 将差值转换为 uint32_t 类型，只保留结果的低 32 位
    uint64_t offset = uint32_t(n - isn);
    if (checkpoint < offset)
        return offset;
    // 1lu << 32 表示 1 向左移 32 位，结果是一个无符号整型(unsigned long)
    offset += ((checkpoint - offset) >> 32) * (1lu << 32);
    return checkpoint - offset <= (1lu << 31) ? offset : offset + (1lu << 32);
}
