#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

int main(int argc, char** argv) {
    static const int port = 3333;

    if (argc != 2) {
        std::cout << "Usage: "sv << argv[0] << " <server IP>"sv << std::endl;
        return 1;
    }

    boost::system::error_code ec;
    auto endpoint = tcp::endpoint(net::ip::make_address(argv[1], ec), port);

    if (ec) {
        std::cout << "Wrong IP format"sv << std::endl;
        return 1;
    }

    net::io_context io_context;
    tcp::socket socket{io_context};
    socket.connect(endpoint, ec);

    if (ec) {
        std::cout << "Can't connect to server"sv << std::endl;
        return 1;
    }

    net::streambuf stream_buf;

    while (true) {
        std::string text;
        if (!std::getline(std::cin, text)) {
            break;   // input ended → exit the loop
        }
        net::write(socket, net::buffer(text + '\n'), ec);

        if (ec) {
            std::cout << "Error sending data"sv << std::endl;
            return 1;
        }

        std::cout << "Sent" << std::endl;

        std::size_t count = net::read_until(socket, stream_buf, '\n', ec);
        if (ec) {
            std::cout << "Error reading data"sv << std::endl;
            return 1;
        }
        auto begin = net::buffers_begin(stream_buf.data());
        std::string server_data(begin, begin + count);

        stream_buf.consume(count);
        std::cout << "Server responded: "sv << server_data << std::endl;
    }
}