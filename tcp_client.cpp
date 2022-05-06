#include <iostream>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;
namespace ip = boost::asio::ip;

static std::string getCurrentTime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  static constexpr size_t MAX_BUFFER_SIZE = 128;
  char buff[MAX_BUFFER_SIZE + 1];
  time_t sec = static_cast<time_t>(tv.tv_sec);
  int ms = static_cast<int>(tv.tv_usec) / 1000;

  struct tm tm_time;
  localtime_r(&sec, &tm_time);
  static const char *formater = "[%4d-%02d-%02d %02d:%02d:%02d.%03d]";
  int ret = snprintf(buff, MAX_BUFFER_SIZE, formater,
                     tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                     tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, ms);
  if (ret < 0) {
    return std::string("");
  }

  return std::string(buff);
}

class client {
public:
    client(boost::asio::io_context& io_service, std::string ip, uint16_t port)
    : ios(io_service), endpoint_(ip::address::from_string(ip), port), socket_(io_service)
    {
    }

    void start() {

        if (!socket_.is_open())
        {
            socket_.open(boost::asio::ip::tcp::v4());
        }
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
        ::snprintf(buf, sizeof(buf) - 1, "local client send cnt: %d\n", cnt++);
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
        std::cout << getCurrentTime() << "recv -> [ " << buf << " ]" << std::endl;

        ::memset(buf, 0, sizeof(buf));
        ::snprintf(buf, sizeof(buf) - 1, "local client send cnt: %d\n", cnt++);

        std::cout << getCurrentTime() << "send -> [ " << buf << " ]" << std::endl;

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
      std::cerr << getCurrentTime() << " usage: tcp_client <remote ip> <remote port>" << std::endl;
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