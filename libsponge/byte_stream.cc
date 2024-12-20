#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// 对于写管道需要实现,write(),remaining_capacity(可写入的写管道的字节量),end_input(结束该写管道),set_error(将写管道设置为error状态)
// 对于读管道需要实现,peek_output(选一个长度为len的数据),pop_output(从缓冲区中弹出len长度的字节，没有返回值),read(从管道中读取len长度的字节),input_ended(与end_input对应，布尔值)
//                  eof(首先写管道关闭，上层调用者不会往写管道中再写入数据；其次要看缓冲区中还有没有数据，如果没有则返回ture)
//                  buffer_size(),buffer_empty(),bytes_written(),bytes_read()
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//这里相当于是python中的实例化类，并且在:后进行成员初始化列表init(),该列表的内容是在byte_stream.cc中private下的成员
//size_t 是一个无符号整数类型，通常用于表示对象的大小或数组的索引
ByteStream::ByteStream(const size_t capacity): 
    _queue(),_capacity_size(capacity),_written_size(0),_read_size(0),_end_input(false),_error(false) {}

size_t ByteStream::write(const string &data) {
    /*
    1、判断是否结束输入
    2、计算要写入的数据大小，是在传入的data大小和缓冲区总量-队列大小中，较小的写入管道
    3、将要写入的值添加到队列_queue后
    4、返回要写入的数据大小
    */
    if (_end_input)
        return 0;

    size_t write_size = min(data.size(), _capacity_size - _queue.size());
    //统计全部已经写入的数据大小
    _written_size += write_size;

    for (size_t i = 0; i < write_size; i++)
        _queue.push_back(data[i]);

    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
//这里的const出现在方法声明末尾，说明该成员函数不会修改类的任何成员变量，意味着peek_output函数是一个常量成员函数
/*
const 成员函数的作用：
保证不修改对象的状态：当一个成员函数声明为 const 时，C++ 会保证该函数不能修改对象的任何非 mutable 成员变量。这是通过将 this 指针转换为 const 指针来实现的，确保函数内部无法通过 this 指针修改对象。
可以在 const 对象上调用：如果你将对象声明为 const，只有 const 成员函数才能被调用。这保证了在不修改对象状态的情况下，可以执行某些操作。
*/
string ByteStream::peek_output(const size_t len) const {
    /*
    从队列中取下个长度为len的数据
    1、确认能取的数据大小
    2、返回队列中该长度的数据
    */
    size_t pop_size = min(len, _queue.size());
    return string(_queue.begin(),_queue.begin() + pop_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    /*
    1、确认要弹出的数据大小，从len和队列中取最小的
    2、累计到已读取的总字节数中
    3、循环弹出队列
    */
    size_t pop_size = min(len, _queue.size());
    _read_size += pop_size;
    for (size_t i = 0; i < pop_size; i++)
        _queue.pop_front();
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    /*
    this->写法相当于python中的self.调用当前类的方式
    1、确定要读取的字符串
    2、弹出该字符串，然后返回弹出的字符串
    */
    string data = this->peek_output(len);
    this->pop_output(len);
    return data;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _queue.size(); }

bool ByteStream::buffer_empty() const { return _queue.empty(); }

/*
1、确保写管道中已关闭
2、并且确保缓冲区中没有数据
*/
bool ByteStream::eof() const { return _end_input && _queue.empty(); }

size_t ByteStream::bytes_written() const { return _written_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity_size - _queue.size(); }
