#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }
  
  // env variable
  string REQUEST_METHOD;
  string REQUEST_URI;
  string QUERY_STRING;
  string SERVER_PROTOCOL;
  string HTTP_HOST;
  string SERVER_ADDR;
  string SERVER_PORT;
  string REMOTE_ADDR;
  string REMOTE_PORT;

  string cgi_file;

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            data_[length] = '\0';
            // see what we got from webpage
            //string data = data_;
            //cout << "received:" << endl;
            //cout << data << endl;
            
            string data = data_;
            string dummy;
            stringstream ss(data);
            ss >> REQUEST_METHOD;  /* GET              */
            ss >> REQUEST_URI;     /* /console.cgi?h0=... */
            ss >> SERVER_PROTOCOL; /* HTTP/1.1 */
            ss >> dummy;           /* Host: */
            ss >> HTTP_HOST;       /* nplinux6.cs.nctu.edu.tw:7777 */
            
            cout << "REQUEST_METHOD: " << REQUEST_METHOD << endl;
            cout << "REQUEST_URI: " << REQUEST_URI << endl;
            cout << "SERVER_PROTOCOL: " << SERVER_PROTOCOL << endl;
            cout << "HTTP_HOST: " << HTTP_HOST << endl;
            
            // get cgi_file and QUERY_STRING from REQUEST_URI
            string request = REQUEST_URI.substr(1);  // remove "/"
            stringstream ss2(request);
            vector<string> tmp;
            string s;
            while (getline(ss2, s, '?')) {
              tmp.push_back(s);
            }

            cgi_file = tmp[0];
            if (tmp.size() == 2){
              QUERY_STRING = tmp[1];
            }
            cout << "QUERY_STRING: " << QUERY_STRING << endl;
            cout << "cgi_file: " << cgi_file << endl;

            do_write(length);
          }
        });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    char status[] = "HTTP/1.1 200 OK\n";
    boost::asio::async_write(socket_, boost::asio::buffer(status, strlen(status)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            SERVER_ADDR = socket_.local_endpoint().address().to_string();
            SERVER_PORT = to_string(socket_.local_endpoint().port());
            REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
            REMOTE_PORT = to_string(socket_.remote_endpoint().port());
            cout << "SERVER_ADDR: " << SERVER_ADDR << endl;
            cout << "SERVER_PORT: " << SERVER_PORT << endl;
            cout << "REMOTE_ADDR: " << REMOTE_ADDR << endl;
            cout << "REMOTE_PORT: " << REMOTE_PORT << endl;

            // set all env variable
            setenv("REQUEST_METHOD", REQUEST_METHOD.c_str(), 1);
            setenv("REQUEST_URI", REQUEST_URI.c_str(), 1);
            setenv("QUERY_STRING", QUERY_STRING.c_str(), 1);
            setenv("SERVER_PROTOCOL", SERVER_PROTOCOL.c_str(), 1);
            setenv("HTTP_HOST", HTTP_HOST.c_str(), 1);
            setenv("SERVER_ADDR", SERVER_ADDR.c_str(), 1);
            setenv("SERVER_PORT", SERVER_PORT.c_str(), 1);
            setenv("REMOTE_ADDR", REMOTE_ADDR.c_str(), 1);
            setenv("REMOTE_PORT", REMOTE_PORT.c_str(), 1);
            
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            // child: execute cgi
            if (fork() == 0) {
              io_context.notify_fork(boost::asio::io_context::fork_child);
              int sock = socket_.native_handle();
              dup2(sock, 0);
              dup2(sock, 1);
              dup2(sock, 2);
              socket_.close();
              
              string exe_path = "./" + cgi_file; 
              if (execlp(exe_path.c_str(), exe_path.c_str(), NULL) < 0) {
                cout << "execlp failed." << endl;
              }
              exit(0);
            }
            // parent 
            else {
              io_context.notify_fork(boost::asio::io_context::fork_parent);
              socket_.close();
            }
            
            //do_read();
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    //boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

