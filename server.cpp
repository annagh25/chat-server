#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(int id, tcp::socket socket)
        : id_(id), socket_(std::move(socket)) {}   // Session owns the socket

    void Start() { DoRead(); }

private:
    void DoRead() {
        net::async_read_until(socket_, stream_buf_, '\n',
            [self = shared_from_this()](const boost::system::error_code& ec, std::size_t count) {
                self->OnRead(ec, count);
            });
    }

    void OnRead(const boost::system::error_code& ec, std::size_t count) {
        if (ec == net::error::eof) {
            std::cout << "Client "sv << id_ << " disconnected"sv << std::endl;
            return;                                  // no new op -> Session gets destroyed
        }
        if (ec) {
            std::cout << "Error reading data: "sv << ec.message() << std::endl;
            return;
        }

        auto begin = net::buffers_begin(stream_buf_.data());
        write_buf_.assign(begin, begin + count);     // includes the '\n'
        stream_buf_.consume(count);                  // consume only this line

        std::cout << "Client "sv << id_ << " said: "sv << write_buf_ << std::flush;

        net::async_write(socket_, net::buffer(write_buf_),   // write_buf_ is a member -> stays alive
            [self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cout << "Error sending data: "sv << ec.message() << std::endl;
                    return;
                }
                self->DoRead();                      // chain: next read after the write completes
            });
    }

    int id_;
    tcp::socket socket_;
    net::streambuf stream_buf_;
    std::string write_buf_;
};

void DoAccept(tcp::acceptor& acceptor, int& next_id) {
    acceptor.async_accept(
        [&acceptor, &next_id](const boost::system::error_code& ec, tcp::socket socket) {
            if (ec) {
                std::cout << "Can't accept connection: "sv << ec.message() << std::endl;
            } else {
                std::cout << "Connected"sv << std::endl;
                std::make_shared<Session>(next_id++, std::move(socket))->Start();
            }
            DoAccept(acceptor, next_id);             // chain: accept the next client
        });
}

int main() {
    static const int port = 3333;

    net::io_context io_context;

    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Waiting for connection..."sv << std::endl;

    int next_id = 0;
    DoAccept(acceptor, next_id);

    io_context.run();
    return 0;
}