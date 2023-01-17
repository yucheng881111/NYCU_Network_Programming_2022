#include <boost/units/absolute.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <bits/stdc++.h>

using boost::asio::ip::tcp;
using namespace std;

class server_info {
public:
	string id;
	string name;
	string port;
	string file;
	server_info(string id, string name, string port, string file) : id(id), name(name), port(port), file(file) {}
};

class session : public std::enable_shared_from_this<session> {
public:
	tcp::socket socket_;        // proxy
	tcp::endpoint endpoint_;    // proxy
	server_info server;
	enum { max_length = 1024 };
	char data_[max_length];
	fstream file;
	session(server_info server, tcp::socket socket_, tcp::endpoint endpoint_) : server(server), socket_(move(socket_)), endpoint_(move(endpoint_)) {
		file.open("test_case/" + server.file, ios::in);
	}

	void start() {
		Connent();
	}

	void Connent() {
		auto self(shared_from_this());
		// connect to proxy
		socket_.async_connect(
			endpoint_, [this, self](const boost::system::error_code &ec){
				if (!ec) {
					//self->do_read();
					self->send_socks4a_request();
				}
			}
		);

	}

	void send_socks4a_request() {
		auto self(shared_from_this());

		// construct Request packet
		char SOCKS4_Request[100];
		memset(SOCKS4_Request, '\0', sizeof(SOCKS4_Request));

		SOCKS4_Request[0] = 4;
		SOCKS4_Request[1] = 1;
		SOCKS4_Request[2] = stoi(server.port) / 256;
		SOCKS4_Request[3] = stoi(server.port) % 256;
		SOCKS4_Request[4] = 0;
		SOCKS4_Request[5] = 0;
		SOCKS4_Request[6] = 0;
		SOCKS4_Request[7] = 1;
		SOCKS4_Request[8] = NULL;
		int i = 9;
		for (int j = 0; j < server.name.size(); ++j) {
			SOCKS4_Request[i] = server.name[j];
			i++;
		}
		SOCKS4_Request[i] = NULL;
		int packet_len = i;

		boost::asio::async_write(socket_, boost::asio::buffer(SOCKS4_Request, packet_len),
			[this, self](boost::system::error_code ec, std::size_t /*length*/) {
				if (!ec) {
					self->receive_socks4a_reply();
				}
		});

	}

	void receive_socks4a_reply() {
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
				if (!ec) {
					// if accept
					if (data_[1] == 90) {
						self->do_read();
					}
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
					//if (cmd != "exit") {
						self->do_read();
					//}
				}
			}
		);

	}

	// in sample_console.cgi
	void output_shell(string session, string content) {
		content = boost::property_tree::xml_parser::encode_char_entities(content);
		boost::replace_all(content, "\n", "&NewLine;");
		printf("<script>document.getElementById('%s').innerHTML += '%s';</script>", session.c_str(), content.c_str());
		cout << flush;
    }

	// in sample_console.cgi
	void output_command(string session, string content) {
		content = boost::property_tree::xml_parser::encode_char_entities(content);
		boost::replace_all(content, "\n", "&NewLine;");
		printf("<script>document.getElementById('%s').innerHTML += '<b>%s</b>';</script>", session.c_str(), content.c_str());
		cout << flush;
    }
};

vector<server_info> servers;

// in sample_console.cgi
void sample_console() {
	printf("Content-type: text/html\r\n\r\n");
	printf(" \
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
	for (auto &server : servers) {
		printf("<th scope=\"col\">%s:%s</th>", server.name.c_str(), server.port.c_str());
	}
	printf(" \
	        </tr> \
	      </thead> \
	      <tbody> \
	        <tr> \
	");
	for (auto &server : servers) {
		printf("<td><pre id=\"%s\" class=\"mb-0\"></pre></td>", server.id.c_str());
	}
	printf(" \
	        </tr> \
	      </tbody> \
	    </table> \
	  </body> \
	</html> \
	");
}


int main() {
	// http_server and cgi communicated by env variable
	string query = getenv("QUERY_STRING");
	// ex: h0=nplinux1.cs.nctu.edu.tw&p0=1234&f0=t1.txt&h1=nplinux2.cs.nctu.edu.tw&p1=5678&f1=t2.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&sh=nplinux3.cs.nctu.edu.tw&sp=12345
	// parse server
	stringstream ss(query);
	string s;
	vector<string> vec_server_name(5);
	vector<string> vec_server_port(5);
	vector<string> vec_server_file(5);
	string proxy_server_ip;
	string proxy_server_port;

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
		} else if (s[0] == 's' && s[1] == 'h') {
			sscanf(s.c_str(), "sh=%s", buffer);
			proxy_server_ip = buffer;
		} else if (s[0] == 's' && s[1] == 'p') {
			sscanf(s.c_str(), "sp=%s", buffer);
			proxy_server_port = buffer;
		}
	}

	for (int i = 0; i < 5; ++i) {
		if (!vec_server_name[i].empty() && !vec_server_port[i].empty() && !vec_server_file[i].empty()) {
			server_info server("s"+to_string(i), vec_server_name[i], vec_server_port[i], vec_server_file[i]);
			servers.push_back(server);
		}
	}
	
	sample_console();
	boost::asio::io_context ioc;
	boost::asio::ip::tcp::resolver resolver(ioc);

	for (auto &server : servers) {
		// host: server name (ex: nplinux1.cs.nctu.edu.tw)
		// port: server port (ex: 8080)
		//auto endpoints = resolver.resolve(tcp::v4(), server.name, server.port);

		// resolve SOCKS proxy server
		auto endpoints = resolver.resolve(tcp::v4(), proxy_server_ip, proxy_server_port);
		tcp::socket socket(ioc);
		make_shared<session>(server, move(socket), endpoints.begin()->endpoint())->start();
	}
	ioc.run();

	return 0;
}

