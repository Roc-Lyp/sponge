#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <vector>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream
//! (possibly out of order, possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    struct Datum {
        char ch = 0;
        bool valid = false;
    };
    // 用于存放未按序达到的字节流
    std::vector<Datum> buffer_;  // buffer to store unassembled data.
    size_t buffer_header_;       // pointer to the begining of the unssembled bytes in
                                 // buffer.
    size_t unassembled_bytes_;
    size_t eof_byte_;
    bool is_eof_set_;    // true if read the eof
    // 存放按序到达的字节流,但是这部分字节流还没有被read
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

  public:
    //! \brief Construct a `StreamReassembler` that will store up to
    //! `capacity` bytes. \note This capacity limits both the bytes that
    //! have been reassembled, and those that have not yet been
    //! reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes
    //! into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the
    //! `capacity`. Bytes that would exceed the capacity are silently
    //! discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first
    //! byte in `data` \param eof the last byte of `data` will be the last
    //! byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet
    //! reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than
    //! once, it should only be counted once for the purpose of this
    //! function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH