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

constexpr std::size_t kMaxLineSize = 4 * 1024;   // bytes, including the '\n'
constexpr auto kIdleTimeout = 30s;               // no line for this long -> disconnect
constexpr std::size_t kMaxQueueBytes = 1024 * 1024;   // unsent bytes per client before "too slow"

// All handlers run on the socket's strand (the socket is accepted onto one),
// so socket_, stream_buf_, write_queue_, queued_bytes_, idle_timer_ and closed_ need no locks.
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
    void ResetIdleTimer();
    void OnIdleTimeout();
    void Close();                        // strand only; the single cleanup path, runs once

    const int id_;                       // const: read from other threads by Room
    Room& room_;
    tcp::socket socket_;
    net::streambuf stream_buf_{kMaxLineSize};   // read fails with not_found instead of growing forever
    std::deque<std::string> write_queue_;   // front() is being written right now
    std::size_t queued_bytes_ = 0;          // total size of write_queue_, in bytes
    net::steady_timer idle_timer_;          // on the socket's strand (see constructor)
    bool closed_ = false;
};

#endif //RADIO_SESSION_H
