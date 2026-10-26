#include "Session.h"

Session::Session(int id, Room& room, tcp::socket socket)
    : id_(id), room_(room), socket_(std::move(socket)),   // Session owns the socket
      idle_timer_(socket_.get_executor()) {}  // same strand as the socket -> no race with read handlers

void Session::Start() {
    // Start is called from the accept handler, not from our strand. After Join, other
    // sessions can Deliver to us, so Join and the first read must also run on our strand.
    net::dispatch(socket_.get_executor(), [self = shared_from_this()] {
        self->room_.Join(self);                  // not in the constructor: shared_from_this needs an owner
        self->DoRead();
    });
}

int Session::Id() const { return id_; }

void Session::Deliver(std::string message) {
    // Called from other sessions' strands: hop onto ours before touching write_queue_.
    net::post(socket_.get_executor(),
        [self = shared_from_this(), msg = std::move(message)]() mutable {
            if (self->closed_) {
                return;                          // client is gone, drop the message
            }
            if (self->queued_bytes_ + msg.size() > kMaxQueueBytes) {   // count bytes, not messages
                std::cout << "Client "sv << self->id_ << " too slow, disconnecting"sv << std::endl;
                self->Close();
                return;
            }
            bool writing = !self->write_queue_.empty();
            self->queued_bytes_ += msg.size();
            self->write_queue_.push_back(std::move(msg));
            if (!writing) {
                self->DoWrite();                 // only one async_write at a time
            }
        });
}

void Session::DoWrite() {
    // deque::push_back doesn't move existing elements, so front() stays valid during the write
    net::async_write(socket_, net::buffer(write_queue_.front()),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (!self->closed_) {            // after Close() the abort is expected, not an error
                    std::cout << "Client "sv << self->id_ << " write error: "sv << ec.message() << std::endl;
                }
                self->Close();
                return;
            }
            self->queued_bytes_ -= self->write_queue_.front().size();
            self->write_queue_.pop_front();      // only now: the bytes are fully sent
            if (!self->write_queue_.empty()) {
                self->DoWrite();
            }
        });
}

void Session::DoRead() {
    ResetIdleTimer();                            // runs at start and after every line
    net::async_read_until(socket_, stream_buf_, '\n',
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t count) {
            self->OnRead(ec, count);
        });
}

void Session::ResetIdleTimer() {
    idle_timer_.expires_after(kIdleTimeout);     // cancels the previous wait -> it gets operation_aborted
    idle_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
        if (ec == net::error::operation_aborted) {
            return;                              // timer was reset or cancelled: NOT a timeout
        }
        self->OnIdleTimeout();
    });
}

void Session::OnIdleTimeout() {
    if (closed_) {
        return;
    }
    std::cout << "Client "sv << id_ << " idle timeout"sv << std::endl;

    // Best effort "* timeout": only if no queued write is in progress (else bytes would mix),
    // and non-blocking, so a dead or stuck client can't block this thread.
    if (write_queue_.empty()) {
        static const std::string notice = "* timeout\n";
        boost::system::error_code ignored;
        socket_.non_blocking(true, ignored);
        socket_.write_some(net::buffer(notice), ignored);   // may send nothing; that's fine
    }
    Close();
}

void Session::Close() {
    if (closed_) {
        return;                                  // read, write and timer can all end up here
    }
    closed_ = true;

    idle_timer_.cancel();                        // its handler drops self -> no 30s zombie
    boost::system::error_code ignored;
    socket_.shutdown(tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);                      // pending read/write fail with operation_aborted
    // Don't clear write_queue_ here: an aborted async_write may still point at front()
    // until its handler runs. The queue is freed with the Session.

    room_.Leave(shared_from_this());             // exactly once -> "left" broadcast once
}

void Session::OnRead(const boost::system::error_code& ec, std::size_t count) {
    if (ec == net::error::eof) {
        std::cout << "Client "sv << id_ << " disconnected"sv << std::endl;
        Close();
        return;
    }
    if (ec == net::error::not_found) {           // buffer full, still no '\n'
        std::cout << "Client "sv << id_ << " line too long, disconnecting"sv << std::endl;
        Close();
        return;
    }
    if (ec) {
        if (!closed_) {                          // after Close() the abort is expected, not an error
            std::cout << "Client "sv << id_ << " read error: "sv << ec.message() << std::endl;
        }
        Close();
        return;
    }

    auto begin = net::buffers_begin(stream_buf_.data());
    std::string line(begin, begin + count);      // includes the '\n'
    stream_buf_.consume(count);                  // consume only this line

    std::cout << "Client "sv << id_ << " said: "sv << line << std::flush;

    room_.Broadcast("["s + std::to_string(id_) + "]: "s + line, id_);
    DoRead();                                    // read the next line right away, don't wait for writes
}
