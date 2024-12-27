#include "stream_reassembler.hh"
#include <cassert>
#include <algorithm>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassemble_strs()
    , _next_assembled_idx(0)
    , _unassembled_bytes_num(0)
    , _eof_idx(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

/*
    1、index <= _next_assembled_idx && index + data.size() > _next_assembled_idx
    2、index > _next_assembled_idx
*/
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // case 1 index <= _next_assembled_idx && index + data.size() > _next_assembled_idx
    /*
    index    _next_assembled_idx   index+data.size()   
    V               V                   V 
    --+-----------------------------------+-----------------------------
    |///////////////|///////////////////|
    --+-----------------------------------+-----------------------------    
    */
    if (index <= _next_assembled_idx && index + data.size() > _next_assembled_idx) {
        size_t overlap_len = _next_assembled_idx - index;
        size_t new_data_size = data.size() - overlap_len;
        if (new_data_size > 0) {
            const string new_data = data.substr(overlap_len, new_data_size);
            return process_data(new_data, index, eof);
        }
    }

    // case 2 index > _nex_assembled_idx
    if (index > _next_assembled_idx) {
        // Find data with smaller index and larger index
        auto up_it = _unassemble_strs.lower_bound(index);
        auto down_it = _unassemble_strs.upper_bound(index);

        // case 2.1.1 up_idx + up_data.size <= index , no handle
        /*
        _next_assembled_idx  up_idx                  index  
                  V            V    up_data.size      V 
        ----------+------------+------------------+---+-----------------...
                  |            |++++++++++++++++++|   |\\\\\\\\\\\\\\\\\...
        ----------+------------+------------------+---+-----------------...
        */
        // case 2.1.2 up_idx + up_data.size > index
        /*
        _next_assembled_idx  up_idx       up_idx+up_data.size     
                  V            V                  V 
        ----------+------------+-----------+------+-----------------
                  |            |+++++++++++|+/+/+/|\\\\\\\\\\\\\\\\\
        ----------+------------+-----------+------+-----------------
                                           A
                                         index    |<-------------->|
        */
        // if there is data before the current data(up_data)
        if (up_it != _unassemble_strs.end()) {
            const auto& [up_idx, up_data] = *up_it;
            // 2.1.1 no overlap, process new data
            if (up_idx + up_data.size() <= index) {
                process_data(up_data, up_idx, eof);
            } else {
                // 2.1.2 overlap, truncate the data
                size_t overlap_size = up_idx + up_data.size() - index;
                size_t new_data_size = data.size() - overlap_size;
                if (new_data_size > 0) {
                    const string new_data = data.substr(overlap_size, new_data_size);
                    return process_data(new_data, up_idx + up_data.size() ,eof);
                }
            }
        }
        
        // case 2.2.1 index + data.size <= down_idx , no overlap, no handler
        /*
         index      index+data.size  down_idx
           V               V            V
        ---+---------------+------------+------------------
           |///////////////|            |++++++++++++++++++
        ---+---------------+------------+------------------
        */
        /*
        case 2.2.2 another two case 
        1、part overlap
        2、complete overlap
        */
        // case 2.2.2.1 index + data.size < down_idx + down_data.size , part overlap need truncate data
        /*
         index              index+data.size   
           V                       V  
        ---+---------------+-------+------------+-----
           |///////////////|+/+/+/+|++++++++++++|     
        ---+---------------+-------+------------+-----
                           A                    A
                        down_idx       down_idx+down_data.size
        */       
        // case 2.2.2.2 index + data.size > down_idx + down_data.size, complete overlap
        /*
         index                          index+data.size   
           V                                    V  
        ---+-----+--------------------+---------+-----+------
           |/////|/+/+/+/+/+/+/+/+/+/+|/////////|     |++++++
        ---+-----+--------------------+---------+-----+------
                 A                    A                 
              down_idx       down_idx+down_data.size
        */
        if (down_it != _unassemble_strs.begin()) {
            --down_it; // move down_it to the neatrest lower bound
            const auto& [down_idx, down_data] = *down_it;
            // 2.2.1 no overlap
            if (index + data.size() <= down_idx) {
                process_data(data, index, eof);
            } else if (index + data.size() > down_idx) {
                // 2.2.2.1 part overlap
                if (index + data.size() < down_idx + down_data.size()) {
                    size_t overlap_size = down_idx + down_data.size() - (index + data.size());
                    const string new_data = data.substr(0, data.size() - overlap_size);
                    process_data(new_data, index, eof);
                } else {
                    // 2.2.2.2 complete overlap
                    _unassembled_bytes_num -= down_data.size();
                    _unassemble_strs.erase(down_it);
                    process_data(data, index, eof);
                }

            }

        }


    }
}


void StreamReassembler::process_data(const string &data, const size_t index, const bool eof) {
    // Add data to the output stream if fits
    if (_output.write(data)) {
        _next_assembled_idx = index + data.size();
    } else {
        // if output is full, store the data 
        _unassemble_strs[index] = data;
        _unassembled_bytes_num += data.size();
    }

    if (eof) {
        _eof_idx = index + data.size();
    }
    assemble_unassembled_data();
}


void StreamReassembler::assemble_unassembled_data() {
    // Try to assemble data starting from _next_assembled_idx
    while (!_unassemble_strs.empty() && _unassemble_strs.begin()->first == _next_assembled_idx) {
        const auto& [idx, data] = *_unassemble_strs.begin();
        if (_output.write(data)) {
            _next_assembled_idx = idx + data.size();
            _unassembled_bytes_num -= data.size();
            _unassemble_strs.erase(_unassemble_strs.begin());
        } else {
            break; // No space in output, stop processing
        }
    }
}


size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_bytes_num; 
}

bool StreamReassembler::empty() const {
    return _unassembled_bytes_num == 0 && _unassemble_strs.empty();
}
