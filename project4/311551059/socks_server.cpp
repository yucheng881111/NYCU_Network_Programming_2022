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

bool pass_firewall(string test_cmd, string test_ip) {
  fstream fin("socks.conf", ios::in);
  vector<string> lines;
  string line;
  while (getline(fin, line)) {
    lines.push_back(line);
  }

  fin.close();

  vector<string> rules_for_b;
  vector<string> rules_for_c;

  for (auto &v : lines) {
    stringstream ss(v);
    string tmp;
    vector<string> vec;
    while (ss >> tmp) {
      vec.push_back(tmp);
    }
    string ip = vec[2];
    if (vec[1] == "b") {
      rules_for_b.push_back(ip);
    } else if (vec[1] == "c") {
      rules_for_c.push_back(ip);
    }
  }

  // parse ip
  vector<string> vec_ip(4);
  string s;
  stringstream ss2(test_ip);
  int i = 0;
  while (getline(ss2, s, '.')) {
    vec_ip[i] = s;
    i++;
  }
  string s1 = vec_ip[0];
  string s2 = vec_ip[1];
  string s3 = vec_ip[2];
  string s4 = vec_ip[3];

  if (test_cmd == "b") {
    for (auto &rules : rules_for_b) {
      vector<string> vec_ip_rule(4);
      string s;
      stringstream ss3(rules);
      int i = 0;
      while (getline(ss3, s, '.')) {
        vec_ip_rule[i] = s;
        i++;
      }
      string rule_s1 = vec_ip_rule[0];
      string rule_s2 = vec_ip_rule[1];
      string rule_s3 = vec_ip_rule[2];
      string rule_s4 = vec_ip_rule[3];
      
      // if pass s1 && pass s2 && pass s3 && pass s4
      if ((rule_s1 == "*" || rule_s1 == s1) && (rule_s2 == "*" || rule_s2 == s2) && (rule_s3 == "*" || rule_s3 == s3) && (rule_s4 == "*" || rule_s4 == s4)) {
        //cout << "pass" << endl;
        return 1;
      }
    }

  } else {
    for (auto &rules : rules_for_c) {
      vector<string> vec_ip_rule(4);
      string s;
      stringstream ss3(rules);
      int i = 0;
      while (getline(ss3, s, '.')) {
        vec_ip_rule[i] = s;
        i++;
      }
      string rule_s1 = vec_ip_rule[0];
      string rule_s2 = vec_ip_rule[1];
      string rule_s3 = vec_ip_rule[2];
      string rule_s4 = vec_ip_rule[3];
      
      // if pass s1 && pass s2 && pass s3 && pass s4
      if ((rule_s1 == "*" || rule_s1 == s1) && (rule_s2 == "*" || rule_s2 == s2) && (rule_s3 == "*" || rule_s3 == s3) && (rule_s4 == "*" || rule_s4 == s4)) {
        //cout << "pass" << endl;
        return 1;
      }
    }
  }

  //cout << "reject" << endl;
  return 0;
}

boost::asio::io_context io_context;

class session
  : public std::enable_shared_from_this<session>
{
public:
  tcp::socket dst_socket;
  tcp::endpoint dst_endpoint;

  session(tcp::socket socket)
    : socket_(std::move(socket)), dst_socket(io_context) {
      memset(data_from_web, 0, max_length);
      memset(data_from_dst, 0, max_length);
    }

  void start()
  {
    do_read();
  }
  /*
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
  */
private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            /*
            data_[length] = '\0';
            // see what we got from webpage
            string data = data_;
            cout << "received:" << endl;
            cout << data << endl;
            */
            int VN = (unsigned char)data_[0];
            int CD = (unsigned char)data_[1];
            int DSTPORT = (unsigned char)data_[2] * 256 + (unsigned char)data_[3];
            string DSTIP = to_string((unsigned char)data_[4]) + "." + to_string((unsigned char)data_[5]) + "." + to_string((unsigned char)data_[6]) + "." + to_string((unsigned char)data_[7]);
            int i;
            string USERID = "";
            for (i = 8; data_[i] != '\0'; i++) {
                USERID += data_[i];
            }
            i++;
            string DOMAIN_NAME = "";
            for (; data_[i] != '\0'; i++) {
                DOMAIN_NAME += data_[i];
            }

            if (VN == 4) {
              string S_IP = socket_.remote_endpoint().address().to_string();
              string S_PORT = to_string(socket_.remote_endpoint().port());

              //cout << "VN: "<< VN << endl;
              //cout << "CD: "<< CD << endl;
              //cout << "<S_IP>: " << S_IP << endl;
              //cout << "<S_PORT>: " << S_PORT << endl;
              //cout << "<D_IP>: " << DSTIP << endl;
              //cout << "<D_PORT>: "<< DSTPORT << endl;
              //cout << "USERID: " << USERID << endl;
              //cout << "DOMAIN_NAME: " << DOMAIN_NAME << endl;

              if (!DOMAIN_NAME.empty()) {
                tcp::resolver resolver(io_context);
                tcp::resolver::query query(tcp::v4(), DOMAIN_NAME, to_string(DSTPORT));
                tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                // Get the first endpoint in the list
                tcp::endpoint endpoint = *endpoint_iterator;
                DSTIP = endpoint.address().to_string();
                DSTPORT = endpoint.port();
              }

              cout << "<S_IP>: " << S_IP << endl;
              cout << "<S_PORT>: " << S_PORT << endl;
              cout << "<D_IP>: " << DSTIP << endl;
              cout << "<D_PORT>: "<< DSTPORT << endl;
              
              dst_endpoint.address(boost::asio::ip::address_v4::from_string(DSTIP));
              dst_endpoint.port(DSTPORT);
              
              if (CD == 1) {
                cout << "<Command>: CONNECT" << endl;
                // need to check firewall before async_connect
                if (pass_firewall("c", DSTIP)) {
                  Connent();
                } else {
                  cout << "<Reply>: Reject" << endl << endl;
                  // construct Reply packet
                  char SOCKS4_REPLY[8];
                  SOCKS4_REPLY[0] = 0;
                  SOCKS4_REPLY[1] = 91; // Reject
                  SOCKS4_REPLY[2] = 0;
                  SOCKS4_REPLY[3] = 0;
                  SOCKS4_REPLY[4] = 0;
                  SOCKS4_REPLY[5] = 0;
                  SOCKS4_REPLY[6] = 0;
                  SOCKS4_REPLY[7] = 0;
                  send_reply_only(SOCKS4_REPLY, &socket_);
                }
                

              } else if (CD == 2) {
                cout << "<Command>: BIND" << endl;
                // need to check firewall before bind
                if (pass_firewall("b", DSTIP)) {
                  Bind();
                } else {
                  cout << "<Reply>: Reject" << endl << endl;
                  // construct Reply packet
                  char SOCKS4_REPLY[8];
                  SOCKS4_REPLY[0] = 0;
                  SOCKS4_REPLY[1] = 91; // Reject
                  SOCKS4_REPLY[2] = 0;
                  SOCKS4_REPLY[3] = 0;
                  SOCKS4_REPLY[4] = 0;
                  SOCKS4_REPLY[5] = 0;
                  SOCKS4_REPLY[6] = 0;
                  SOCKS4_REPLY[7] = 0;
                  send_reply_only(SOCKS4_REPLY, &socket_);
                }
                
              }

              //do_write(length);
            }
            

          } else {
            //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
          }
        });
  }

  void Connent() {
    auto self(shared_from_this());
    dst_socket.async_connect(
      dst_endpoint, [this, self](const boost::system::error_code &ec){
        if (!ec) {
          cout << "<Reply>: Accept" << endl << endl;
          // construct Reply packet
          char SOCKS4_REPLY[8];
          SOCKS4_REPLY[0] = 0;
          SOCKS4_REPLY[1] = 90; // Accept
          SOCKS4_REPLY[2] = 0;
          SOCKS4_REPLY[3] = 0;
          SOCKS4_REPLY[4] = 0;
          SOCKS4_REPLY[5] = 0;
          SOCKS4_REPLY[6] = 0;
          SOCKS4_REPLY[7] = 0;

          send_reply_to_web(SOCKS4_REPLY, &socket_);

        } else {
          //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
        }
      }
    );
  }

  void Bind() {
    auto self(shared_from_this());
    auto bind_acceptor_ = std::make_shared<tcp::acceptor>(io_context, tcp::endpoint(tcp::v4(), 0));  // port 0 means assign any available port
    int bind_port = bind_acceptor_->local_endpoint().port();
    // construct Reply packet
    char SOCKS4_REPLY[8];
    SOCKS4_REPLY[0] = 0;
    SOCKS4_REPLY[1] = 90; // Accept
    SOCKS4_REPLY[2] = bind_port / 256;
    SOCKS4_REPLY[3] = bind_port % 256;
    SOCKS4_REPLY[4] = 0;
    SOCKS4_REPLY[5] = 0;
    SOCKS4_REPLY[6] = 0;
    SOCKS4_REPLY[7] = 0;
    send_reply_only(SOCKS4_REPLY, &socket_);

    // accept connection from FTP server (active mode)
    bind_acceptor_->async_accept(
      [this, self, bind_acceptor_, bind_port](boost::system::error_code ec, tcp::socket socket) {
          if (!ec) {
            cout << "<Reply>: Accept" << endl << endl;
            self->dst_socket = std::move(socket);
            char SOCKS4_REPLY2[8];
            SOCKS4_REPLY2[0] = 0;
            SOCKS4_REPLY2[1] = 90; // Accept
            SOCKS4_REPLY2[2] = bind_port / 256;
            SOCKS4_REPLY2[3] = bind_port % 256;
            SOCKS4_REPLY2[4] = 0;
            SOCKS4_REPLY2[5] = 0;
            SOCKS4_REPLY2[6] = 0;
            SOCKS4_REPLY2[7] = 0;
            send_reply_to_web(SOCKS4_REPLY2, &socket_);
          }

      });

  }

  void send_reply_only(char* msg, tcp::socket *web_socket) {
    auto self(shared_from_this());
    boost::asio::async_write(*web_socket, boost::asio::buffer(msg, 8),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            //cout << "Reply sent" << endl << endl;
          }
        });
  }

  void send_reply_to_web(char* msg, tcp::socket *web_socket) {
    auto self(shared_from_this());
    boost::asio::async_write(*web_socket, boost::asio::buffer(msg, 8),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            //cout << "Reply sent" << endl << endl;
            //read_from(&socket_, &dst_socket, "web");
            //read_from(&dst_socket, &socket_, "dst");
            read_from("web");
            read_from("dst");
          }
        });


  }

  void read_from(/*tcp::socket *src, tcp::socket *dst, */string where) {
    auto self(shared_from_this());

    if (where == "web") {
      socket_.async_read_some(boost::asio::buffer(data_from_web, max_length),
        [this, self, where](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              
              self->write_to(length, where);
              
            } else if (ec == boost::asio::error::eof) {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
              /*
              if (where == "web") {
                self->socket_.close();
              } else {
                self->dst_socket.close();
              }
              */
              
              dst_socket.async_send(boost::asio::buffer(data_from_web, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec) {}
              });
              dst_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);

            } else {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
            }

      });
    } else {
      dst_socket.async_read_some(boost::asio::buffer(data_from_dst, max_length),
        [this, self, where](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              
              self->write_to(length, where);
              
            } else if (ec == boost::asio::error::eof) {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
              /*
              if (where == "web") {
                self->socket_.close();
              } else {
                self->dst_socket.close();
              }
              */
              
              socket_.async_send(boost::asio::buffer(data_from_dst, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec) {}
              });
              socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);

            } else {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
            }

      });
    }


    
  }


  void write_to(/*tcp::socket *src, tcp::socket *dst, char *buffer, */int buffer_len, string where) {
    auto self(shared_from_this());
    if (where == "web") {
      async_write(dst_socket, boost::asio::buffer(data_from_web, buffer_len),
        [this, self, where](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec) {
              memset(data_from_web, 0, max_length);
              self->read_from(where);
            } else {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
            }
          
      });
    } else {
      async_write(socket_, boost::asio::buffer(data_from_dst, buffer_len),
        [this, self, where](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec) {
              memset(data_from_dst, 0, max_length);
              self->read_from(where);
            } else {
              //cout << ec.category().name() << " : " << ec.value() << " : " << ec.message() << endl;
            }
          
      });
    }
    
  }

  
  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    char status[] = "HTTP/1.1 200 OK\n";
    boost::asio::async_write(socket_, boost::asio::buffer(status, strlen(status)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {}
        });
  }
  

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  char data_from_web[max_length];
  char data_from_dst[max_length];
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
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            // child
            if (fork() == 0) {
              io_context.notify_fork(boost::asio::io_context::fork_child);
              this->acceptor_.close();
              std::make_shared<session>(std::move(socket))->start();
              //exit(0);
            }
            // parent 
            else {
              io_context.notify_fork(boost::asio::io_context::fork_parent);
              socket.close();
            }
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
    //std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

