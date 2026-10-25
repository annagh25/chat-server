#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Session.h"
#include "Room.h"

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

void DoAccept(Room& room, tcp::acceptor& acceptor, int& next_id) {
    acceptor.async_accept(net::make_strand(acceptor.get_executor()),
        [&room, &acceptor, &next_id](const boost::system::error_code& ec, tcp::socket socket) {
            if (ec) {
                std::cout << "Can't accept connection: "sv << ec.message() << std::endl;
            } else {
                std::cout << "Connected"sv << std::endl;
                std::make_shared<Session>(next_id++, room, std::move(socket))->Start();
            }
            DoAccept(room, acceptor, next_id);             // chain: accept the next client
        });
}

int main() {
    static const int port = 3333;

    net::io_context io_context;

    Room room;

    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Waiting for connection..."sv << std::endl;

    int next_id = 0;
    DoAccept(room, acceptor, next_id);

    static const int num_threads = 4;
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads - 1; ++i) {
        workers.emplace_back([&io_context] { io_context.run(); });
    }
    io_context.run();                            // main thread is the 4th worker

    for (auto& worker : workers) {
        worker.join();
    }
    return 0;
}