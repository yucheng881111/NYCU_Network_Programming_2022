#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/units/absolute.hpp>
#include <boost/asio/signal_set.hpp>
#include <bits/stdc++.h>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

class server_info {
public:
  string id;
  string name;
  string port;
  string file;
  server_info(string id, string name, string port, string file) : id(id), name(name), port(port), file(file) {}
};

class client_session : public std::enable_shared_from_this<client_session> {
public:
  tcp::socket socket_;
  tcp::socket *web_socket_;
  tcp::endpoint endpoint_;
  server_info server;
  enum { max_length = 1024 };
  char data_[max_length];
  fstream file;
  client_session(tcp::socket *web_socket_, server_info server, tcp::socket socket_, tcp::endpoint endpoint_) : web_socket_(web_socket_), server(server), socket_(move(socket_)), endpoint_(move(endpoint_)) {
    file.open("test_case/" + server.file, ios::in);
  }

  void start() {
    Connent();
  }

  void Connent() {
    auto self(shared_from_this());
    socket_.async_connect(
      endpoint_, [this, self](const boost::system::error_code &ec){
        if (!ec) {
          self->do_read();
        }
      }
    );

  }

  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          data_[length] = '\0';
          string data = data_;
          //cout << "received:" << endl;
          //cout << data << endl;
          self->output_shell(self->server.id, data);
          // check if is time to send
          size_t found = data.find("% ");
          if (found != string::npos) {
            self->do_write();
          } else {
            self->do_read();
          }
        }
      }

    );
  }

  void do_write() {
    auto self(shared_from_this());
    // get one line each time from test_case.txt
    string cmd;
    if (getline(file, cmd)) {
      if (cmd.size() && cmd.back() == '\r') {
        cmd.pop_back();
      }
      cmd += "\n";
      output_command(server.id, cmd);
    } else {
      return ;
    }
    
    socket_.async_write_some(boost::asio::buffer(cmd),
      [this, self, cmd](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          if (cmd != "exit") {
            self->do_read();
          }
        }
      }
    );

  }

  // in sample_console.cgi
  void output_shell(string session, string content) {
    content = boost::property_tree::xml_parser::encode_char_entities(content);
    boost::replace_all(content, "\n", "&NewLine;");
    char buf[20000];
    memset(buf, '\0', sizeof(buf));
    sprintf(buf, "<script>document.getElementById('%s').innerHTML += '%s';</script>", session.c_str(), content.c_str());
    //cout << flush;
    boost::asio::async_write(*web_socket_, boost::asio::buffer(buf, strlen(buf)),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {}
        });
  }

  // in sample_console.cgi
  void output_command(string session, string content) {
    content = boost::property_tree::xml_parser::encode_char_entities(content);
    boost::replace_all(content, "\n", "&NewLine;");
    char buf[20000];
    memset(buf, '\0', sizeof(buf));
    sprintf(buf, "<script>document.getElementById('%s').innerHTML += '<b>%s</b>';</script>", session.c_str(), content.c_str());
    //cout << flush;
    boost::asio::async_write(*web_socket_, boost::asio::buffer(buf, strlen(buf)),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {}
        });
  }
};

// in sample_console.cgi
string sample_console(vector<server_info> servers) {
  string result;
  char buffer[20000];
  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, "Content-type: text/html\r\n\r\n");
  result += string(buffer);

  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
  <!DOCTYPE html> \
  <html lang=\"en\"> \
    <head> \
      <meta charset=\"UTF-8\" /> \
      <title>NP Project 3 Sample Console</title> \
      <link \
        rel=\"stylesheet\" \
        href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\" \
        integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\" \
        crossorigin=\"anonymous\" \
      /> \
      <link \
        href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" \
        rel=\"stylesheet\" \
      /> \
      <link \
        rel=\"icon\" \
        type=\"image/png\" \
        href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" \
      /> \
      <style> \
        * { \
          font-family: 'Source Code Pro', monospace; \
          font-size: 1rem !important; \
        } \
        body { \
          background-color: #212529; \
        } \
        pre { \
          color: #cccccc; \
        } \
        b { \
          color: #01b468; \
        } \
      </style> \
    </head> \
    <body> \
      <table class=\"table table-dark table-bordered\"> \
        <thead> \
          <tr> \
  ");
  result += string(buffer);
  
  for (auto &server : servers) {
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "<th scope=\"col\">%s:%s</th>", server.name.c_str(), server.port.c_str());
    result += string(buffer);
  }

  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
          </tr> \
        </thead> \
        <tbody> \
          <tr> \
  ");
  result += string(buffer);

  for (auto &server : servers) {
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "<td><pre id=\"%s\" class=\"mb-0\"></pre></td>", server.id.c_str());
    result += string(buffer);
  }

  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
          </tr> \
        </tbody> \
      </table> \
    </body> \
  </html> \
  ");
  result += string(buffer);

  return result;
}


// turn python form panel.cgi into c++ form
string panel_cgi() {
  string result;
  int N_SERVERS = 5;

  string FORM_METHOD = "GET";
  string FORM_ACTION = "console.cgi";

  string TEST_CASE_DIR = "test_case";
  /*
  try:
      test_cases = sorted(os.listdir(TEST_CASE_DIR))
  except:
      test_cases = []
  test_case_menu = ''.join([f'<option value="{test_case}">{test_case}</option>' for test_case in test_cases])
  */

  string test_case_menu;
  /*
  for (const auto &entry : std::filesystem::directory_iterator(TEST_CASE_DIR)) {
    string test_case = entry.path();
    test_case = test_case.substr(10);
    test_case_menu += "<option value=\"" + test_case + "\">" + test_case + "</option>";
  }
  */

  for (int i = 0; i < 100; ++i) {
    string test_case = "test_case/t" + to_string(i) + ".txt";
    ifstream ifile;
    ifile.open(test_case);
    if (ifile) {
      test_case = test_case.substr(10);
      test_case_menu += "<option value=\"" + test_case + "\">" + test_case + "</option>";
    }
  }

  string DOMAIN_ = "cs.nctu.edu.tw";
  /*
  hosts = [f'nplinux{i + 1}' for i in range(12)]
  host_menu = ''.join([f'<option value="{host}.{DOMAIN}">{host}</option>' for host in hosts])
  */
  string host_menu;
  for (int i = 1; i <= 12; ++i) {
    string host = "nplinux" + to_string(i);
    host_menu += "<option value=\"" + host + "." + DOMAIN_ + "\">" + host + "</option>";
  }

  char buffer[20000];
  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, "Content-type: text/html\r\n\r\n");
  result += string(buffer);

  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
  <!DOCTYPE html> \
  <html lang=\"en\"> \
    <head> \
      <title>NP Project 3 Panel</title> \
      <link \
        rel=\"stylesheet\" \
        href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\" \
        integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\" \
        crossorigin=\"anonymous\" \
      /> \
      <link \
        href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" \
        rel=\"stylesheet\" \
      /> \
      <link \
        rel=\"icon\" \
        type=\"image/png\" \
        href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\" \
      /> \
      <style> \
        * { \
          font-family: 'Source Code Pro', monospace; \
        } \
      </style> \
    </head> \
    <body class=\"bg-secondary pt-5\"> \
  ");
  result += string(buffer);

  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, "<form action=\"%s\" method=\"%s\">", FORM_ACTION.c_str(), FORM_METHOD.c_str());
  result += string(buffer);
  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
        <table class=\"table mx-auto bg-light\" style=\"width: inherit\"> \
          <thead class=\"thead-dark\"> \
            <tr> \
              <th scope=\"col\">#</th> \
              <th scope=\"col\">Host</th> \
              <th scope=\"col\">Port</th> \
              <th scope=\"col\">Input File</th> \
            </tr> \
          </thead> \
          <tbody> \
  ");
  result += string(buffer);

  for (int i = 0; i < N_SERVERS; ++i) {
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, " \
            <tr> \
              <th scope=\"row\" class=\"align-middle\">Session %d</th> \
              <td> \
                <div class=\"input-group\"> \
                  <select name=\"h%d\" class=\"custom-select\"> \
                    <option></option>%s \
                  </select> \
                  <div class=\"input-group-append\"> \
                    <span class=\"input-group-text\">.cs.nctu.edu.tw</span> \
                  </div> \
                </div> \
              </td> \
              <td> \
                <input name=\"p%d\" type=\"text\" class=\"form-control\" size=\"5\" /> \
              </td> \
              <td> \
                <select name=\"f%d\" class=\"custom-select\"> \
                  <option></option> \
                  %s \
                </select> \
              </td> \
            </tr> \
    ", i+1, i, host_menu.c_str(), i, i, test_case_menu.c_str());
    result += string(buffer);
  }
      
  memset(buffer, '\0', sizeof(buffer));
  sprintf(buffer, " \
            <tr> \
              <td colspan=\"3\"></td> \
              <td> \
                <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button> \
              </td> \
            </tr> \
          </tbody> \
        </table> \
      </form> \
    </body> \
  </html> \
  ");
  result += string(buffer);

  return result;
}


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
            /*
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
            */
            //do_read();
          }
        });

    if (cgi_file == "panel.cgi") {
      string result = panel_cgi();
      boost::asio::async_write(socket_, boost::asio::buffer(result.c_str(), strlen(result.c_str())),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {}
        });

    } else if (cgi_file == "console.cgi") {
      vector<server_info> servers;
      stringstream ss(QUERY_STRING);
      string s;
      vector<string> vec_server_name(5);
      vector<string> vec_server_port(5);
      vector<string> vec_server_file(5);

      while (getline(ss, s, '&')) {
        char buffer[30];
        memset(buffer, '\0', sizeof(buffer));
        if (s[0] == 'h') {
          int session_id;
          sscanf(s.c_str(), "h%d=%s", &session_id, buffer);
          vec_server_name[session_id] = buffer;
        } else if (s[0] == 'p') {
          int port_num;
          sscanf(s.c_str(), "p%d=%s", &port_num, buffer);
          vec_server_port[port_num] = buffer;
        } else if (s[0] == 'f') {
          int file_num;
          sscanf(s.c_str(), "f%d=%s", &file_num, buffer);
          vec_server_file[file_num] = buffer;
        }
      }

      for (int i = 0; i < 5; ++i) {
        if (!vec_server_name[i].empty() && !vec_server_port[i].empty() && !vec_server_file[i].empty()) {
          server_info server("s"+to_string(i), vec_server_name[i], vec_server_port[i], vec_server_file[i]);
          servers.push_back(server);
        }
      }

      string result = sample_console(servers);
      boost::asio::async_write(socket_, boost::asio::buffer(result.c_str(), strlen(result.c_str())),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {}
        });

      boost::asio::io_context ioc;
      boost::asio::ip::tcp::resolver resolver(ioc);

      for (auto &server : servers) {
        // host: server name (ex: nplinux1.cs.nctu.edu.tw)
        // port: server port (ex: 8080)
        auto endpoints = resolver.resolve(tcp::v4(), server.name, server.port);
        tcp::socket socket(ioc);
        make_shared<client_session>(&socket_, server, move(socket), endpoints.begin()->endpoint())->start();
      }
      ioc.run();
    }
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class Server
{
public:
  Server(boost::asio::io_context& io_context, short port)
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

int main_(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    //boost::asio::io_context io_context;

    Server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

#include <conio.h>

int main(int argc, char* argv[]) {
  std::thread t(main_, argc, argv);

  while (char c = _getch()) {
    cout << c << endl;
    if (c == 'c') {
      exit(0);
    }
  };

  t.join();
}
