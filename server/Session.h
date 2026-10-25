#ifndef RADIO_SESSION_H
#define RADIO_SESSION_H

#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <string>

#include "Room.h"

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

// All handlers run on the socket's strand (the socket is accepted onto one),
// so socket_, stream_buf_ and write_queue_ need no locks.
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(int id, Room& room, tcp::socket socket);
    void Start();
    int Id() const;
    void Deliver(std::string message);   // thread-safe: posts onto this session's strand

private:
    void DoRead();
    void OnRead(const boost::system::error_code& ec, std::size_t count);
    void DoWrite();                      // writes write_queue_.front()

    const int id_;                       // const: read from other threads by Room
    Room& room_;
    tcp::socket socket_;
    net::streambuf stream_buf_;
    std::deque<std::string> write_queue_;   // front() is being written right now
};

#endif //RADIO_SESSION_H
