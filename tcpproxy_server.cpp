//
// tcpproxy_server.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2007 Arash Partow (http://www.partow.net)
// URL: http://www.partow.net/programming/tcpproxy/index.html
//
// Distributed under the Boost Software License, Version 1.0.
//
//
// Description
// ~~~~~~~~~~~
// The  objective of  the TCP  proxy server  is to  act  as  an
// intermediary  in order  to 'forward'  TCP based  connections
// from external clients onto a singular remote server.
//
// The communication flow in  the direction from the  client to
// the proxy to the server is called the upstream flow, and the
// communication flow in the  direction from the server  to the
// proxy  to  the  client   is  called  the  downstream   flow.
// Furthermore  the   up  and   down  stream   connections  are
// consolidated into a single concept known as a bridge.
//
// In the event  either the downstream  or upstream end  points
// disconnect, the proxy server will proceed to disconnect  the
// other  end  point  and  eventually  destroy  the  associated
// bridge.
//
// The following is a flow and structural diagram depicting the
// various elements  (proxy, server  and client)  and how  they
// connect and interact with each other.

//
//                                    ---> upstream --->           +---------------+
//                                                     +---->------>               |
//                               +-----------+         |           | Remote Server |
//                     +--------->          [x]--->----+  +---<---[x]              |
//                     |         | TCP Proxy |            |        +---------------+
// +-----------+       |  +--<--[x] Server   <-----<------+
// |          [x]--->--+  |      +-----------+
// |  Client   |          |
// |           <-----<----+
// +-----------+
//                <--- downstream <---
//
//


#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <memory>
#include <mutex>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

namespace tcp_proxy
{
   namespace ip = boost::asio::ip;

   class bridge : public std::enable_shared_from_this<bridge>
   {
   public:

      typedef ip::tcp::socket socket_type;
      typedef std::shared_ptr<bridge> ptr_type;

      bridge(boost::asio::io_service& ios)
      : downstream_socket_(ios),
        upstream_socket_  (ios)
      {}

      socket_type& downstream_socket()
      {
         // Client socket
         return downstream_socket_;
      }

      socket_type& upstream_socket()
      {
         // Remote server socket
         return upstream_socket_;
      }

      void start()
      {
         ip::tcp::endpoint remote_ep = downstream_socket_.remote_endpoint();
         std::cout << "remote ip: " << remote_ep.address().to_string() << " port: " << remote_ep.port() << std::endl;;

         remote_ep = upstream_socket_.remote_endpoint();
         std::cout << "the other remote ip: " << remote_ep.address().to_string() << " port: " << remote_ep.port() << std::endl;
         
         ::memset(downstream_data_, 0, max_data_length);
         ::memset(upstream_data_, 0, max_data_length);
         
         handle_upstream_connect();
      }

      void handle_upstream_connect()
      {
         // Setup async read from remote server (upstream)
         upstream_socket_.async_read_some(
               boost::asio::buffer(upstream_data_,max_data_length),
               boost::bind(&bridge::handle_upstream_read,
                     shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));

         // Setup async read from client (downstream)
         downstream_socket_.async_read_some(
               boost::asio::buffer(downstream_data_,max_data_length),
               boost::bind(&bridge::handle_downstream_read,
                     shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));

      }

   private:

      /*
         Section A: Remote Server --> Proxy --> Client
         Process data recieved from remote sever then send to client.
      */

      // Read from remote server complete, now send data to client
      void handle_upstream_read(const boost::system::error_code& error,
                                const size_t& bytes_transferred)
      {
         if (!error)
         {
            async_write(downstream_socket_,
                 boost::asio::buffer(upstream_data_,bytes_transferred),
                 boost::bind(&bridge::handle_downstream_write,
                      shared_from_this(),
                      boost::asio::placeholders::error));
         }
         else {
            std::cout << error.message() << std::endl;
            close();
         }
      }

      // Write to client complete, Async read from remote server
      void handle_downstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            upstream_socket_.async_read_some(
                 boost::asio::buffer(upstream_data_,max_data_length),
                 boost::bind(&bridge::handle_upstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else {
            std::cout << error.message() << std::endl;
            close();
         }
      }
      // *** End Of Section A ***


      /*
         Section B: Client --> Proxy --> Remove Server
         Process data recieved from client then write to remove server.
      */

      // Read from client complete, now send data to remote server
      void handle_downstream_read(const boost::system::error_code& error,
                                  const size_t& bytes_transferred)
      {
         if (!error)
         {
            async_write(upstream_socket_,
                  boost::asio::buffer(downstream_data_,bytes_transferred),
                  boost::bind(&bridge::handle_upstream_write,
                        shared_from_this(),
                        boost::asio::placeholders::error));
         }
         else {
            std::cout << error.message() << std::endl;
            close();
         }
      }

      // Write to remote server complete, Async read from client
      void handle_upstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            downstream_socket_.async_read_some(
                 boost::asio::buffer(downstream_data_,max_data_length),
                 boost::bind(&bridge::handle_downstream_read,
                      shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
         }
         else {
            std::cout << error.message() << std::endl;
            close();
         }
      }
      // *** End Of Section B ***

      void close()
      {
         std::scoped_lock lock(mutex_);

         if (downstream_socket_.is_open())
         {
            downstream_socket_.close();
         }

         if (upstream_socket_.is_open())
         {
            upstream_socket_.close();
         }

         ::memset(downstream_data_, 0, max_data_length);
         ::memset(upstream_data_, 0, max_data_length);
         std::cout << "close all socket on the bridge" << std::endl;
      }

      socket_type downstream_socket_;
      socket_type upstream_socket_;

      enum { max_data_length = 8192 }; //8KB
      unsigned char downstream_data_[max_data_length];
      unsigned char upstream_data_[max_data_length];

      std::mutex mutex_;

   public:

      class acceptor
      {
      public:

         acceptor(boost::asio::io_service& io_service,
                  const std::string& local_host, unsigned short local_port)
         : io_service_(io_service),
           localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
           acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port))
         {}

         bool accept_connections()
         {
            try
            {
               session_ = std::shared_ptr<bridge>(new bridge(io_service_));

               acceptor_.async_accept(session_->downstream_socket(),
                    boost::bind(&acceptor::accept_other_connections,
                         this,
                         boost::asio::placeholders::error));
            }
            catch(std::exception& e)
            {
               std::cerr << "acceptor exception: " << e.what() << std::endl;
               return false;
            }

            return true;
         }

         bool accept_other_connections(const boost::system::error_code& error) {
            if(error) {
               std::cout << error.message() << std::endl;
               return false;
            }

            try
            {
               acceptor_.async_accept(session_->upstream_socket(),
                     boost::bind(&acceptor::handle_accept,
                        this,
                        boost::asio::placeholders::error));
            }
            catch(std::exception& e)
            {
               std::cerr << "acceptor exception: " << e.what() << std::endl;
               return false;
            }

            return true;
         }

      private:

         void handle_accept(const boost::system::error_code& error)
         {
            if (!error)
            {
               session_->start();

               if (!accept_connections())
               {
                  std::cerr << "Failure during call to accept." << std::endl;
               }
            }
            else
            {
               std::cerr << "Error: " << error.message() << std::endl;
            }
         }

         boost::asio::io_service& io_service_;
         ip::address_v4 localhost_address;
         ip::tcp::acceptor acceptor_;
         ptr_type session_;
         unsigned short upstream_port_;
         std::string upstream_host_;
      };

   };
}

int main(int argc, char* argv[])
{
   if (argc != 3)
   {
      std::cerr << "usage: tcpproxy_server <local host ip> <local port>" << std::endl;
      return 1;
   }

   const std::string local_host = argv[1];
   const unsigned short local_port = static_cast<unsigned short>(::atoi(argv[2]));

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::bridge::acceptor acceptor(ios, local_host, local_port);

      acceptor.accept_connections();

      ios.run();
   }
   catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}

/*
 * [Note] On posix systems the tcp proxy server build command is as follows:
 * c++ -pedantic -ansi -Wall -Werror -O3 -o tcpproxy_server tcpproxy_server.cpp -L/usr/lib -lstdc++ -lpthread -lboost_thread -lboost_system
 */
