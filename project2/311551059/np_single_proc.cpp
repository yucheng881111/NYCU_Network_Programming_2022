#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>

using namespace std;

class global_client {
public:
	int fd;
	int id;
	string name;
	global_client(int fd, int id, string name) : fd(fd), id(id), name(name) {}
};

vector<global_client> clients_copy;

class UserPipe {
public:
	struct pipeInfo {
		int sender_id;
		int receiver_id;
		int fd_in;
		int fd_out;
		string command_msg;
	};

	vector<pipeInfo> all_user_pipes;

	bool create_user_pipe(int s_id, int r_id, string cmd) {
		for (auto &p : all_user_pipes) {
			if (p.sender_id == s_id && p.receiver_id == r_id) {
				return 0;
			}
		}

		int currfd[2] = {-1, -1};
		pipe(currfd);
		pipeInfo info;
		info.sender_id = s_id;
		info.receiver_id = r_id;
		info.fd_in = currfd[0];
		info.fd_out = currfd[1];
		info.command_msg = cmd;
		all_user_pipes.push_back(info);
		cout << "s_id:" << s_id << " r_id:" << r_id << " pipe created." << " f_in:" << currfd[0] << " f_out:" << currfd[1] << endl;

		return 1;
	}

	void set_userpipe_send(int s_id, int r_id, int &stdout) {
		for (auto &p : all_user_pipes) {
			if (p.sender_id == s_id && p.receiver_id == r_id) {
				stdout = p.fd_out;
				break;
			}
		}
	}

	void set_userpipe_receive(int s_id, int r_id, int &stdin) {
		for (auto &p : all_user_pipes) {
			if (p.sender_id == s_id && p.receiver_id == r_id) {
				stdin = p.fd_in;
				break;
			}
		}
	}

	void close_used_pipes(int s_id, int r_id) {
		vector<pipeInfo>::iterator iter;
		bool flag = 0;
		for (iter = all_user_pipes.begin(); iter != all_user_pipes.end(); ++iter) {
			if (iter->sender_id == s_id && iter->receiver_id == r_id) {
				close(iter->fd_in);
				close(iter->fd_out);
				flag = 1;
				break;
			}
		}
		if (flag) {
			all_user_pipes.erase(iter);
		}
	}

	void clean_all() {
		for (auto &p : all_user_pipes) {
			close(p.fd_in);
			close(p.fd_out);
		}
	}

	void clean_logout(int fd) {
		int id;
		for (auto &c : clients_copy) {
			if (c.fd == fd) {
				id = c.id;
				break;
			}
		}

		auto iter = all_user_pipes.begin();
		while (iter != all_user_pipes.end()) {
			if (iter->sender_id == id || iter->receiver_id == id) {
				close(iter->fd_in);
				close(iter->fd_out);
				all_user_pipes.erase(iter);
				iter = all_user_pipes.begin();
			} else {
				iter++;
			}
		}
	}
};

UserPipe userPipe;

class shell {
public:
	int fd;
	int id;

	class Env {
	public:
		map<string, string> env_map;
		void init() {
			set("PATH", "bin:.");
		}

		void set(string key, string value) {
			env_map[key] = value;
		}

		string get(string key) {
			if (env_map.count(key)) {
				return env_map[key];
			} else {
				return "";
			}
		}
		/*
		bool check_cmd_in_path(string cmd) {
			string var_path = env_map["PATH"];
			// split by ':'
			stringstream ss(var_path);
			string tmp;
			vector<string> all_path;
			while (getline(ss, tmp, ':')) {
				all_path.push_back(tmp);
			}
			return 1;
		}
		*/
		string get_path() {
			return env_map["PATH"];
		}
			
	};

	class OrdinaryPipe {
	public:
		int pipe_cnt = 0;
		int before[2] = {0, 1};
		int after[2] = {0, 1};
		vector<vector<int>> all_pipes;

		void open_pipe(vector<int> &currfd) {
			currfd[0] = before[0];
			if (pipe_cnt) {
				pipe(after);
				all_pipes.push_back({after[0], after[1]});
				pipe_cnt--;
				currfd[1] = after[1];
			}
			
		}


		void clean_before() {
			if (before[0] != 0) {
				close(before[0]);
			}
			
			if (before[1] != 1) {
				close(before[1]);
			}
		}

		void clean_after() {
			if (after[0] != 0) {
				close(after[0]);
			}
			
			if (after[1] != 1) {
				close(after[1]);
			}
		}

		void update() {
			before[0] = after[0];
			before[1] = after[1];
		}

		void clean_all() {
			for (auto &p : all_pipes) {
				close(p[0]);
				close(p[1]);
			}
		}
	};


	class NumberPipe {
	public:

		// {curr_no, dest_no, stdin, stdout}
		vector<vector<int>> all_outer_pipes;
		int curr_line_no = 0;

		void create_outer_pipe(int jump_no) {
			int currfd[2] = {-1, -1};

			int dest_no = jump_no + curr_line_no;

			// check if pipe of dest_no is already exist
			vector<int> tmp;
			for (auto &p : all_outer_pipes) {
				if (p[1] == dest_no) {
					tmp.assign(p.begin(), p.end());
				}
			}

			
			if (tmp.empty()) {
				// pipe not created yet
				pipe(currfd);
			} else {
				// pipe has been created
				currfd[0] = tmp[2];
				currfd[1] = tmp[3];	
			}

			all_outer_pipes.push_back({curr_line_no, dest_no, currfd[0], currfd[1]});
		}

		void set_pipe_input(int &stdin) {
			// check if curr_line is dest
			for (auto &p : all_outer_pipes) {
				if (curr_line_no == p[1]) {
					stdin = p[2];
					break;
				}
			}

		}

		void set_pipe_output(int &stdout) {
			// find curr_line output pipe
			for (auto &p : all_outer_pipes) {
				if (curr_line_no == p[0]) {
					stdout = p[3];
					break;
				}
			}

		}

		void close_used_pipes() {
			// check if curr_line is dest
			for (auto &p : all_outer_pipes) {
				if (curr_line_no == p[1]) {
					close(p[2]);
					close(p[3]);
				}
			}
		}

		void clean_all() {
			for (auto &p : all_outer_pipes) {
				close(p[2]);
				close(p[3]);
			}
		}
	};

	Env env;
	NumberPipe OuterPipes;
	bool writing_file_mode = 0;
	string writing_file_name = "";
	bool stderr_to_stdout = 0;
	bool has_userpipe_send = 0;
	int curr_receiver_id = 0;
	bool has_userpipe_receive = 0;
	int curr_sender_id = 0;
	bool sender_invalid = 0;
	bool receiver_invalid = 0;
	string broadcast_receive_msg = "";
	string broadcast_pipe_msg = "";

	void reset_flag() {
		writing_file_mode = 0;
		writing_file_name = "";
		stderr_to_stdout = 0;
		has_userpipe_send = 0;
		curr_receiver_id = 0;
		has_userpipe_receive = 0;
		curr_sender_id = 0;
		sender_invalid = 0;
		receiver_invalid = 0;
		broadcast_receive_msg = "";
		broadcast_pipe_msg = "";
	}

	class Command {
	public:
		vector<string> cmd;
		Command(vector<string>& vec): cmd(vec) {}

	};


	pid_t ForkProcess(vector<string> &cmd, vector<int> &currfd, OrdinaryPipe &Pipe, string exe_path) {
		string command = cmd[0];
		
		char** argv = new char*[cmd.size()+1];
		for (int i = 0; i < cmd.size(); i++) {
			argv[i] = const_cast<char*>(cmd[i].c_str());
		}
		argv[cmd.size()] = NULL;
		
		/*
		for (int i = 0; i < cmd.size(); ++i) {
			cout << argv[i] << " ";
		}
		cout << endl;
		*/

		int stdin = currfd[0];
		int stdout = currfd[1];
		int stderr = currfd[2];
		
		int childpid = fork();
		if (childpid == 0) {  // child

			setenv("PATH", exe_path.c_str(), 1);

			// same as np_simple
			close(0);
            close(1);
            close(2);
            dup(fd);
            dup(fd);
            dup(fd);

			if (stdin != 0) {
				close(0);
				dup(stdin);
			}

			if (stdout == -1) {
				close(1);
			} else if (stdout != 1) {
				close(1);
				dup(stdout);
			}

			if (stderr == -1) {
				close(2);
			} else if (stderr != 2) {
				close(2);
				dup(stderr);
			}

			Pipe.clean_all();
			OuterPipes.clean_all();
			userPipe.clean_all();

			if(execvp(command.c_str(), argv) < 0){
				cerr << "Unknown command: [" << command << "]." << endl;
				//printf("execl returned! errno is [%d]\n", errno);
				//perror("The error message is : ");

			}
			exit(0);

		} else {
			
		}

		return childpid;
	}

	void HandleOneLineCmd(vector<Command> &cmd_list, bool has_outer_pipe) {
		// brodcast
		if (broadcast_receive_msg != "") {
			for (auto &c : clients_copy) {
				write(c.fd, broadcast_receive_msg.c_str(), strlen(broadcast_receive_msg.c_str()));
			}
		}

		if (broadcast_pipe_msg != "") {
			for (auto &c : clients_copy) {
				write(c.fd, broadcast_pipe_msg.c_str(), strlen(broadcast_pipe_msg.c_str()));
			}
		}
		
		OrdinaryPipe Pipe;
		Pipe.pipe_cnt = cmd_list.size() - 1;
		int childpid = -1;
		int previous_childpid = -1;
		int writing_file_fd = -1;
		
		cout << "full cmd:" << endl;
		for (auto &C : cmd_list) {
			for (auto &c : C.cmd) {
				cout << c << " ";
			}
			cout << endl;
		}
		cout << "size: " << cmd_list.size() << endl;

		for (int i = 0; i < cmd_list.size(); i++) {
			vector<int> currfd = {0, 1, 2};
			Pipe.open_pipe(currfd);
			
			if (i == 0) {
				OuterPipes.set_pipe_input(currfd[0]);
			}

			if (i == cmd_list.size() - 1) {
				OuterPipes.set_pipe_output(currfd[1]);
				
				if (stderr_to_stdout) {
					currfd[2] = currfd[1];
				}
				
				if (writing_file_mode) {
					// create(open) a file with write only and truncate, and all permission on
					writing_file_fd = creat(writing_file_name.c_str(), S_IRWXU);
					currfd[1] = writing_file_fd;
				}
			}

			// user pipe
			if (i == 0 && has_userpipe_receive) {
				if (sender_invalid) {
					currfd[0] = open("/dev/null", O_RDONLY);
					cout << "sender_invalid fd: " << currfd[0] << endl;
				} else {
					userPipe.set_userpipe_receive(curr_sender_id, id, currfd[0]);
					cout << "handle sender_id: " << curr_sender_id << " id: " << id << " receive from fd: " << currfd[0] << endl;
				}
			}

			if (i == cmd_list.size() - 1 && has_userpipe_send) {
				if (receiver_invalid) {
					currfd[1] = open("/dev/null", O_WRONLY);
					cout << "receiver_invalid fd: " << currfd[1] << endl;
				} else {
					userPipe.set_userpipe_send(id, curr_receiver_id, currfd[1]);
					cout << "handle id: " << id << " receiver_id: " << curr_receiver_id << " send to fd: " << currfd[1] << endl;
				}
			}
			
			cout << "doing:" << endl;
			for (auto &s : cmd_list[i].cmd) {
				cout << s << " ";
			}
			cout << endl;

			string exe_path = env.get_path();
			childpid = ForkProcess(cmd_list[i].cmd, currfd, Pipe, exe_path);
			cout << "done:" << endl;
			for (auto &s : cmd_list[i].cmd) {
				cout << s << " ";
			}
			cout << endl;


			Pipe.clean_before();             // must go first, close main pipe before wait
			Pipe.update();
			//cout << childpid << endl;
			
			OuterPipes.close_used_pipes();
			if (has_userpipe_receive) {
				userPipe.close_used_pipes(curr_sender_id, id);
			}
			

			// wait every command, pass testcase 5 but not 6 (cat big file)
			//waitpid(childpid, NULL, 0);
			
			// wait previous command
			if (previous_childpid != -1) {
				waitpid(previous_childpid, NULL, 0);
			}
			previous_childpid = childpid;
			
			if (i == cmd_list.size() - 1) {
				
				if (writing_file_mode) {
					close(writing_file_fd);
				}
				
				if (!has_outer_pipe) {
					waitpid(childpid, NULL, 0);
				}
				
			}
			
		}

	}

	void printCommandAndLineNo(vector<Command> &one_line_cmd, int num) {

		for (auto &c : one_line_cmd) {
			for (auto &tmp : c.cmd) {
				cout << tmp;
			}
			cout << " ";
		}

		cout << num;
		cout << "               line: " << OuterPipes.curr_line_no << endl;
	}

	void Parser(string str) {
		stringstream ss(str);
		string s = "";
		vector<string> vec;
		vector<Command> one_line_cmd;
		
		while(ss >> s) {
			if (s[0] == '|' || s[0] == '!' || (s[0] == '>' && s.size() == 1)) {
				Command c(vec);
				one_line_cmd.push_back(c);
				vec.clear();

				if ((s[0] == '|' && s.size() != 1) || s[0] == '!') {  // number pipe
					int num;
					stringstream ss(s.substr(1));
					ss >> num;
					OuterPipes.curr_line_no++;

					//printCommandAndLineNo(one_line_cmd, num);

					if (s[0] == '!') {
						stderr_to_stdout = 1;
					}

					OuterPipes.create_outer_pipe(num);
					HandleOneLineCmd(one_line_cmd, 1);
					reset_flag();
					one_line_cmd.clear();
				}

				if (s[0] == '>' && s.size() == 1) {  // output to file
					OuterPipes.curr_line_no++;
					writing_file_mode = 1;
					ss >> writing_file_name;
					HandleOneLineCmd(one_line_cmd, 0);
					reset_flag();
					one_line_cmd.clear();
				}
				
			} else if (s[0] == '<' && s.size() != 1) {  // user pipe receive
				int s_id;
				stringstream ss(s.substr(1));
				ss >> s_id;
				bool sender_valid = 0;
				has_userpipe_receive = 1;
				for (auto &c : clients_copy) {
					if (c.id == s_id) {
						sender_valid = 1;
						break;
					}
				}
				
				if (!sender_valid) {
					sender_invalid = 1;
					string message = "*** Error: user #" + to_string(s_id) + " does not exist yet. ***\n";
					write(fd, message.c_str(), strlen(message.c_str()));
				} else {
					
					curr_sender_id = s_id;
					//cout << "id:" << id << " s_id: " << s_id << endl;

					// check if pipe exist
					bool pipe_valid = 0;
					for (auto &p : userPipe.all_user_pipes) {
						if (p.sender_id == s_id) {
							pipe_valid = 1;
							break;
						}
					}

					if (pipe_valid) {
						// broadcast message
						string find_my_name;
						string find_sender_name;
						for (auto &c : clients_copy) {
							if (c.id == id) {
								find_my_name = c.name;
							}
							if (c.id == s_id) {
								find_sender_name = c.name;
							}
						}
						//broadcast_receive_msg = "*** " + find_my_name + " (#" + to_string(id) +") just received from " + find_sender_name + " (#" + to_string(s_id) + ") by '"+ str + "' ***\n";
						/*
						for (auto &c : clients_copy) {
							write(c.fd, message.c_str(), strlen(message.c_str()));
						}
						*/
						stringstream ss2(str);
						vector<string> full_cmd;
						string partial;
						broadcast_receive_msg = "*** " + find_my_name + " (#" + to_string(id) +") just received from " + find_sender_name + " (#" + to_string(s_id) + ") by '";
						while (ss2 >> partial) {
							full_cmd.push_back(partial);
						}
						for (int i = 0; i < full_cmd.size(); ++i) {
							if (i == full_cmd.size() - 1) {
								broadcast_receive_msg += full_cmd[i];
							} else {
								broadcast_receive_msg += (full_cmd[i] + " ");
							}
						}
						broadcast_receive_msg += "' ***\n";

					} else {
						string message = "*** Error: the pipe #" + to_string(s_id) +"->#" + to_string(id) + " does not exist yet. ***\n";
						write(fd, message.c_str(), strlen(message.c_str()));
						sender_invalid = 1;
					}
					
				}

			} else if (s[0] == '>' && s.size() != 1) {  // user pipe send
				int r_id;
				stringstream ss(s.substr(1));
				ss >> r_id;
				bool receiver_valid = 0;
				has_userpipe_send = 1;
				for (auto &c : clients_copy) {
					if (c.id == r_id) {
						receiver_valid = 1;
						break;
					}
				}
				
				if (!receiver_valid) {
					receiver_invalid = 1;
					string message = "*** Error: user #" + to_string(r_id) + " does not exist yet. ***\n";
					write(fd, message.c_str(), strlen(message.c_str()));
				} else {
					//cout << "id: " << id << " r_id:" << r_id << endl;
					if (userPipe.create_user_pipe(id, r_id, str)){
						curr_receiver_id = r_id;

						// broadcast message
						string find_my_name;
						string find_receiver_name;
						for (auto &c : clients_copy) {
							if (c.id == id) {
								find_my_name = c.name;
							}
							if (c.id == r_id) {
								find_receiver_name = c.name;
							}
						}
						//broadcast_pipe_msg = "*** " + find_my_name + " (#" + to_string(id) +") just piped '" + str + "' to " + find_receiver_name + " (#" + to_string(r_id) + ") ***\n";
						/*
						for (auto &c : clients_copy) {
							write(c.fd, message.c_str(), strlen(message.c_str()));
						}
						*/
						stringstream ss2(str);
						vector<string> full_cmd;
						string partial;
						broadcast_pipe_msg = "*** " + find_my_name + " (#" + to_string(id) +") just piped '";
						while (ss2 >> partial) {
							full_cmd.push_back(partial);
						}
						for (int i = 0; i < full_cmd.size(); ++i) {
							if (i == full_cmd.size() - 1) {
								broadcast_pipe_msg += full_cmd[i];
							} else {
								broadcast_pipe_msg += (full_cmd[i] + " ");
							}
						}
						broadcast_pipe_msg += ("' to " + find_receiver_name + " (#" + to_string(r_id) + ") ***\n");

					} else {
						string message = "*** Error: the pipe #" + to_string(id) + "->#" + to_string(r_id) + " already exists. ***\n";
						write(fd, message.c_str(), strlen(message.c_str()));
						receiver_invalid = 1;
					}
					
				}

			} else {
				vec.push_back(s);
			}
		}

		if (!vec.empty()) {
			Command c(vec);
			one_line_cmd.push_back(c);
			OuterPipes.curr_line_no++;

			//printCommandAndLineNo(one_line_cmd, 0);

			HandleOneLineCmd(one_line_cmd, 0);
			reset_flag();
			one_line_cmd.clear();
		}

		if (!one_line_cmd.empty()) {
			OuterPipes.curr_line_no++;
			HandleOneLineCmd(one_line_cmd, 0);
			reset_flag();
			one_line_cmd.clear();
		}

		//write(fd, "% ", 2);
	}


	void npshell(string command) {
		
		//setenv("PATH", "bin:.", 1);
		//char* tmp = getenv("PATH");
		//cout << tmp << endl;
		
		string str = command;
		if (str == "") {
			return ;
		}
		
		if (str == "exit") {
			return ;
		}

		stringstream ss(str);
		string s = "";
		ss >> s;
		if (s == "setenv") {
			// build-in command count as one line for numbered pipe
			OuterPipes.curr_line_no++;
			string var = "";
			string value = "";
			ss >> var;
			ss >> value;
			/*
			if (setenv(var.c_str(), value.c_str(), 1) < 0) {
				cout << "setenv error!" << endl;
			}
			*/
			env.set(var, value);
						
		} else if (s == "printenv") {
			// build-in command count as one line for numbered pipe
			OuterPipes.curr_line_no++;
			string var = "";
			ss >> var;
			/*
			char* tmp; 
			if (tmp = getenv(var.c_str())) {
				cout << tmp << endl;
			}
			*/
			string result = env.get(var);
			result += "\n";
			write(fd, result.c_str(), strlen(result.c_str()));
			
		} else {
			Parser(str);
		}
		
	}

};

class client {
public:
	int fd;
	int id;
	string ip;
	int port;
	string name;
	shell sh;

	client(int fd, int id, string ip, int port, string name) : fd(fd), id(id), ip(ip), port(port), name(name) {
		sh.fd = fd;
		sh.id = id;
		sh.env.init();
	}

};

bool cmp(client &a, client &b) {
	return a.id < b.id;
}


void HandleCommand(int client_fd, string cmd, vector<client> &clients) {
	stringstream ss(cmd);
	string tmp;
	ss >> tmp;

	string message = "";
	if (tmp == "who") {
		sort(clients.begin(), clients.end(), cmp);
		message += "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
		for (auto &c : clients) {
			if (c.fd == client_fd) {
				message += (to_string(c.id) + "\t" + c.name + "\t" + c.ip + ":" + to_string(c.port) + "\t" + "<-me\n");
			} else {
				message += (to_string(c.id) + "\t" + c.name + "\t" + c.ip + ":" + to_string(c.port) + "\n");
			}
			
		}
		write(client_fd, message.c_str(), strlen(message.c_str()));
		//write(client_fd, "% ", 2);

	} else if (tmp == "name") {
		string new_name;
		ss >> new_name;

		bool valid = 1;
		for (auto &c : clients) {
			if (c.name == new_name) {
				valid = 0;
				break;
			}
		}

		if (!valid) {
			message += ("*** User '" + new_name + "' already exists. ***\n");
			write(client_fd, message.c_str(), strlen(message.c_str()));
		} else {
			for (auto &c : clients) {
				if (c.fd == client_fd) {
					c.name = new_name;
					message += ("*** User from " + c.ip + ":" + to_string(c.port) + " is named '" + c.name + "'. ***\n");
				}
			}

			for (auto &c : clients_copy) {
				if (c.fd == client_fd) {
					c.name = new_name;
				}
			}

			for (auto &c : clients) {
				write(c.fd, message.c_str(), strlen(message.c_str()));
			}
		}
		//write(client_fd, "% ", 2);

	} else if (tmp == "yell") {
		string who_yell;
		for (auto &c : clients) {
			if (c.fd == client_fd) {
				who_yell = c.name;
			}
		}
		message += ("*** " + who_yell + " yelled ***: ");


		vector<string> remain_msg;
		string msg;
		while (ss >> msg) {
			remain_msg.push_back(msg);
		}

		for (int i = 0; i < remain_msg.size(); ++i) {
			if (i == remain_msg.size() - 1) {
				message += (remain_msg[i] + "\n");
			} else {
				message += (remain_msg[i] + " ");
			}
		}

		for (auto &c : clients) {
			write(c.fd, message.c_str(), strlen(message.c_str()));
		}
		//write(client_fd, "% ", 2);

	} else if (tmp == "tell") {
		int user_id;
		ss >> user_id;
		string who_tell;
		int tell_fd;

		bool valid = 0;
		for (auto &c : clients) {
			if (c.id == user_id) {
				valid = 1;
				tell_fd = c.fd;
			}

			if (c.fd == client_fd) {
				who_tell = c.name;
			}
		}

		if (!valid) {
			message += ("*** Error: user #" + to_string(user_id) + " does not exist yet. ***\n");
			write(client_fd, message.c_str(), strlen(message.c_str()));
		} else {
			vector<string> remain_msg;
			string msg;
			while (ss >> msg) {
				remain_msg.push_back(msg);
			}

			message += ("*** " + who_tell + " told you ***: ");
			for (int i = 0; i < remain_msg.size(); ++i) {
				if (i == remain_msg.size() - 1) {
					message += (remain_msg[i] + "\n");
				} else {
					message += (remain_msg[i] + " ");
				}
			}

			write(tell_fd, message.c_str(), strlen(message.c_str()));
		}
		//write(client_fd, "% ", 2);

	} else {
		// not build-in command, call shell
		stringstream ss2(cmd);
		vector<string> vec;
		string command;
		string tmp_cmd;
		while (ss2 >> tmp_cmd) {
			vec.push_back(tmp_cmd);
		}

		for (int i = 0; i < vec.size(); ++i) {
			if (i == vec.size() - 1) {
				command += vec[i];
			} else {
				command += (vec[i] + " ");
			}
		}

		for (auto &c : clients) {
			if (c.fd == client_fd) {
				c.sh.npshell(command);
			}
		}
		//write(client_fd, "% ", 2);
	}

	write(client_fd, "% ", 2);
}

int main(int argc, char *argv[], char *envp[]) {

	int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int port = atoi(argv[1]);
    int client_socket[30], max_clients = 30;
    int max_sd;

    //set of socket descriptors 
    fd_set readfds;
 
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
 
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 20) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "listening on port " << port << " ......" << endl;

    for (int i = 0; i < max_clients; ++i) {
    	client_socket[i] = 0;
    }

    vector<client> clients;

    while (1) {  
        //clear the socket set 
        FD_ZERO(&readfds);
     
        //add server socket to set 
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
             
        //add child sockets to set
        for (int i = 0 ; i < max_clients ; i++) {  
            //socket descriptor
            int sd = client_socket[i];
                 
            //if valid socket descriptor then add to read list 
            if (sd > 0) {
                FD_SET(sd, &readfds);  
            }
                 
            //highest file descriptor number, need it for the select function
            max_sd = max(max_sd, sd);
        }  
     
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
       
        if ((activity < 0) && (errno != EINTR)) {  
            cout << "select error" << endl;
        }
             
        //If something happened on the master socket , then its an incoming connection 
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }
             
            //inform user of socket number - used in send and receive commands 
            printf("New connection , socket fd is %d , ip is : %s , port : %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            

            //send new connection greeting message
            /*
            ****************************************
			** Welcome to the information server. **
			****************************************
            */
            string message = "****************************************\n** Welcome to the information server. **\n****************************************\n";
            if (write(new_socket, message.c_str(), strlen(message.c_str())) != strlen(message.c_str())) {
                perror("send");
            }
                 
            cout << "Welcome message sent successfully." << endl;
            
            string login_message;
            //add new socket to array of sockets 
            for (int i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    cout << "Adding to list of sockets as " << i << endl;

                    // client(fd, id, ip, port, name)
                    client new_client(new_socket, i+1, inet_ntoa(address.sin_addr), ntohs(address.sin_port), "(no name)");
                    clients.push_back(new_client);

                    global_client new_client_copy(new_socket, i+1, "(no name)");
                    clients_copy.push_back(new_client_copy);

                    login_message = "*** User '(no name)' entered from " + new_client.ip + ":" + to_string(new_client.port) + ". ***\n";

                    break;
                }
            }

            // broadcast login message
            for (auto &c : clients) {
            	write(c.fd, login_message.c_str(), strlen(login_message.c_str()));
            }

            write(new_socket, "% ", 2);
        }
        
        char buffer[20000];
        //else its some IO operation on some other socket
        for (int i = 0; i < max_clients; i++) {
            int sd = client_socket[i];
            if (FD_ISSET(sd, &readfds)) {
                //Check if it was for closing , and also read the incoming message
                int valread;
                if ((valread = read(sd, buffer, 20000)) == 0) {
                    //Somebody disconnected , get his details and print 
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    cout << "received from client " << i << ": " << buffer << endl;
                    string cmd = buffer;
                    stringstream ss(cmd);
                    string cmd2;
                    ss >> cmd2;
                    if (cmd2 == "exit") {
                    	userPipe.clean_logout(sd);

                    	string who_logout;
                    	vector<client>::iterator iter;
                    	for (iter = clients.begin(); iter != clients.end(); ++iter) {
                    		if (iter->fd == sd) {
                    			who_logout = iter->name;
                    			break;
                    		}
                    	}
                    	close(sd);
                    	client_socket[i] = 0;
                    	clients.erase(iter);

                    	vector<global_client>::iterator iter2;
                    	for (iter2 = clients_copy.begin(); iter2 != clients_copy.end(); ++iter2) {
                    		if (iter2->fd == sd) {
                    			break;
                    		}
                    	}
                    	clients_copy.erase(iter2);

                    	// broadcast logout message
                    	string logout_message = "*** User '" + who_logout + "' left. ***\n";
                    	for (auto &c : clients) {
			            	write(c.fd, logout_message.c_str(), strlen(logout_message.c_str()));
			            }
                    	continue;
                    }
                    HandleCommand(sd, cmd, clients);
                }  
            }  
        }  
    }  







return 0;	
}












