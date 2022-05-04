#include <iostream>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;
namespace ip = boost::asio::ip;

class client {
public:
    client(boost::asio::io_context& io_service, std::string ip, uint16_t port)
    : ios(io_service), endpoint_(ip::address::from_string(ip), port), socket_(io_service)
    {
    }

    void start() {
        socket_.non_blocking(true);
        socket_.set_option(boost::asio::ip::tcp::no_delay(true));
        socket_.async_connect(endpoint_, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error));
    }

    void handle_connect(const boost::system::error_code& error) {
        if(error) {
            std::cout << error.message() << std::endl;
            socket_.close();
            return;
        }
        ::memset(buf, 0, sizeof(buf));
        ::snprintf(buf, sizeof(buf) - 1, "client send cnt: %d\n", cnt++);
        boost::asio::async_write(socket_, boost::asio::buffer(buf, strlen(buf)),
                                boost::bind(&client::handle_wirte,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));


    }

    void handle_wirte(const boost::system::error_code& error, size_t size) {
        if(error) {
            std::cout << error.message() << std::endl;
            socket_.close();
            return;
        }

        boost::asio::async_read_until(socket_, sbuf, "\n",
                                    boost::bind(&client::handle_read,
                                    this,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));

    }

    void handle_read(const boost::system::error_code& error, size_t size) {
        if(error) {
            std::cout << error.message() << std::endl;
            socket_.close();
            sbuf.consume(sbuf.size());
            return;
        }

        sbuf.sgetn(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(size));
        std::cout << "recv -> [ " << buf << " ]" << std::endl;

        ::memset(buf, 0, sizeof(buf));
        ::snprintf(buf, sizeof(buf) - 1, "client send cnt: %d\n", cnt++);

        std::cout << "send -> [ " << buf << " ]" << std::endl;

        boost::asio::async_write(socket_, boost::asio::buffer(buf, strlen(buf)),
                                boost::bind(&client::handle_wirte,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));

    }


private:
    boost::asio::io_context& ios;
    tcp::endpoint endpoint_;
    tcp::socket socket_;
    char buf[1024];
    boost::asio::streambuf sbuf;
    int cnt = 0;
};



int main(int argc, char* argv[])
{
    if (argc != 3)
   {
      std::cerr << "usage: tcp_client <remote ip> <remote port>" << std::endl;
      return 1;
   }

   const std::string remote_host = argv[1];
   const uint16_t remote_port = static_cast<uint16_t>(::atoi(argv[2]));

    boost::asio::io_context io_service;

    client cli(io_service, remote_host, remote_port);
    cli.start();

    io_service.run();
    return 0;
}