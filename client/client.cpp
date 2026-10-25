#include <boost/asio.hpp>
#include <atomic>
#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

// All socket work runs on the io_context thread (one thread -> no strand needed).
// The main thread only reads the keyboard and posts lines here.
class Client {
public:
    Client(net::io_context& io_context, tcp::socket socket)
        : io_context_(io_context), socket_(std::move(socket)) {}

    void Start() { DoRead(); }

    // Called from the main thread: hop onto the io_context thread first.
    void Send(std::string line) {
        net::post(io_context_, [this, line = std::move(line)]() mutable {
            bool writing = !write_queue_.empty();
            write_queue_.push_back(std::move(line));
            if (!writing) {
                DoWrite();                       // only one async_write at a time
            }
        });
    }

    // Called from the main thread at end of input: tell the server we're done sending.
    void Finish() {
        net::post(io_context_, [this] {
            finished_ = true;
            if (write_queue_.empty()) {
                Shutdown();                      // else: shut down after the last write
            }
        });
    }

    bool Connected() const { return connected_; }

private:
    void DoRead() {
        net::async_read_until(socket_, stream_buf_, '\n',
            [this](const boost::system::error_code& ec, std::size_t count) {
                if (ec) {
                    if (ec != net::error::eof) {
                        std::cout << "Error reading data: "sv << ec.message() << std::endl;
                    }
                    std::cout << "Disconnected from server"sv << std::endl;
                    connected_ = false;
                    return;                      // no more work -> io_context.run() returns
                }
                auto begin = net::buffers_begin(stream_buf_.data());
                std::string line(begin, begin + count);   // includes the '\n'
                stream_buf_.consume(count);
                std::cout << line << std::flush;
                DoRead();
            });
    }

    void DoWrite() {
        net::async_write(socket_, net::buffer(write_queue_.front()),
            [this](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cout << "Error sending data: "sv << ec.message() << std::endl;
                    write_queue_.clear();
                    return;
                }
                write_queue_.pop_front();        // only now: the bytes are fully sent
                if (!write_queue_.empty()) {
                    DoWrite();
                } else if (finished_) {
                    Shutdown();
                }
            });
    }

    void Shutdown() {
        boost::system::error_code ignored;
        socket_.shutdown(tcp::socket::shutdown_send, ignored);   // server sees eof, we keep reading
    }

    net::io_context& io_context_;
    tcp::socket socket_;
    net::streambuf stream_buf_;
    std::deque<std::string> write_queue_;   // touched only on the io_context thread
    bool finished_ = false;                 // touched only on the io_context thread
    std::atomic<bool> connected_ = true;    // written by io thread, read by main thread
};

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

    Client client(io_context, std::move(socket));
    client.Start();                              // read is pending -> run() won't return early
    std::thread io_thread([&io_context] { io_context.run(); });

    std::string text;
    while (client.Connected() && std::getline(std::cin, text)) {
        client.Send(text + '\n');
    }
    client.Finish();                             // input ended (Ctrl+D) or server left

    io_thread.join();                            // returns once the server closes the connection
    return 0;
}
