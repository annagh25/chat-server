#include "Session.h"

Session::Session(int id, Room& room, tcp::socket socket)
    : id_(id), room_(room), socket_(std::move(socket)) {}   // Session owns the socket

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
            bool writing = !self->write_queue_.empty();
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
                std::cout << "Error sending data: "sv << ec.message() << std::endl;
                self->room_.Leave(self);
                return;
            }
            self->write_queue_.pop_front();      // only now: the bytes are fully sent
            if (!self->write_queue_.empty()) {
                self->DoWrite();
            }
        });
}

void Session::DoRead() {
    net::async_read_until(socket_, stream_buf_, '\n',
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t count) {
            self->OnRead(ec, count);
        });
}

void Session::OnRead(const boost::system::error_code& ec, std::size_t count) {
    if (ec == net::error::eof) {
        std::cout << "Client "sv << id_ << " disconnected"sv << std::endl;
        room_.Leave(shared_from_this());         // Room drops its shared_ptr -> Session can be destroyed
        return;
    }
    if (ec) {
        std::cout << "Error reading data: "sv << ec.message() << std::endl;
        room_.Leave(shared_from_this());
        return;
    }

    auto begin = net::buffers_begin(stream_buf_.data());
    std::string line(begin, begin + count);      // includes the '\n'
    stream_buf_.consume(count);                  // consume only this line

    std::cout << "Client "sv << id_ << " said: "sv << line << std::flush;

    room_.Broadcast("["s + std::to_string(id_) + "]: "s + line, id_);
    DoRead();                                    // read the next line right away, don't wait for writes
}
