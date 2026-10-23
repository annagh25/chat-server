#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

int main() {
    static const int port = 3333;

    net::io_context io_context;

    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Waiting for connection..."sv << std::endl;

    boost::system::error_code ec;
    tcp::socket socket{io_context};
    acceptor.accept(socket, ec);

    if (ec) {
        std::cout << "Can't accept connection"sv << std::endl;
        return 1;
    }

    std::cout << "Connected"sv << std::endl;

    net::streambuf stream_buf;

    while (true) {
        std::size_t count = net::read_until(socket, stream_buf, '\n', ec);

        if (ec == net::error::eof) {
            std::cout << "Client disconnected"sv << std::endl;
            break;
        }

        if (ec) {
            std::cout << "Error reading data"sv << std::endl;
            return 1;
        }

        auto begin = net::buffers_begin(stream_buf.data());
        std::string client_data(begin, begin + count);

        stream_buf.consume(count);

        std::cout << "Client said: "sv << client_data << std::endl;

        net::write(socket, net::buffer(client_data), ec);

        if (ec) {
            std::cout << "Error sending data"sv << std::endl;
            return 1;
        }
    }
}