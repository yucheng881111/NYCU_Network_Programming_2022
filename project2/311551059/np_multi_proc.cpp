#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

using namespace std;

/* have to put infomation into share memory */
int shm_pipes_id;
int shm_clients_id;
int shm_buffer_id;

struct buffer {
	bool lock;
	char broadcast_or_tell[20000];
} *shm_buffer;


int my_id;
bool has_userpipe_send = 0;
bool has_userpipe_receive = 0;
int curr_sender_id = 0;
bool sender_invalid = 0;
bool receiver_invalid = 0;
int write_fd = -1;
int read_fd = -1;
bool writing_file_mode = 0;
string writing_file_name = "";
bool stderr_to_stdout = 0;
string broadcast_pipe_msg = "";
string broadcast_receive_msg = "";

struct UserPipe {
	bool receive[31];
	int sender_open_fd[31];
	int receiver_open_fd[31];
} *all_user_pipes;

void user_pipes_clean_all() {
	for (int i = 1; i < 31; ++i) {
		for (int j = 1; j < 31; ++j) {
			close(all_user_pipes[i].sender_open_fd[j]);
			close(all_user_pipes[i].receiver_open_fd[j]);
		}
	}
}

void remove_userpipe(int curr_sender_id, int my_id) {
	all_user_pipes[curr_sender_id].receive[my_id] = 0;
	all_user_pipes[curr_sender_id].sender_open_fd[my_id] = -1;
	all_user_pipes[curr_sender_id].receiver_open_fd[my_id] = -1;

	string myfifo = "user_pipe/" + to_string(curr_sender_id) + "_to_" + to_string(my_id);
	unlink(myfifo.c_str());
}

void reset_flag() {
	has_userpipe_send = 0;
	has_userpipe_receive = 0;
	curr_sender_id = 0;
	sender_invalid = 0;
	receiver_invalid = 0;
	write_fd = -1;
	read_fd = -1;
	writing_file_mode = 0;
	writing_file_name = "";
	stderr_to_stdout = 0;
	broadcast_pipe_msg = "";
	broadcast_receive_msg = "";
}

struct client {
	int pid;
	char name[30];
	char ip[INET_ADDRSTRLEN];
	char port[10];
} *clients;

void SIGHANDLE(int sig) {
	// broadcast or tell
	if (sig == SIGUSR1) {
		cout << shm_buffer->broadcast_or_tell;
	}
	// receive from user pipe 
	else if (sig == SIGUSR2) {

		// find in share memory
		for (int i = 1; i < 31; ++i) {
			if (all_user_pipes[i].receive[my_id] && all_user_pipes[i].receiver_open_fd[my_id] == -1) {
				string myfifo = "user_pipe/" + to_string(i) + "_to_" + to_string(my_id);
				mkfifo(myfifo.c_str(), 0666);
				// Open FIFO for Read only
				int fd = open(myfifo.c_str(), O_RDONLY);
				all_user_pipes[i].receiver_open_fd[my_id] = fd;
			}
		}

	} else if (sig == SIGCHLD) {
		int status;
		while (waitpid(-1, &status, WNOHANG) > 0);

	} else if (sig == SIGINT) {
		cout << "clean share memory" << endl;
		shmdt(clients);
		shmctl(shm_clients_id, IPC_RMID, 0);
		shmdt(all_user_pipes);
		shmctl(shm_pipes_id, IPC_RMID, 0);
		//shmdt(broadcast_or_tell);
		shmdt(shm_buffer);
		shmctl(shm_buffer_id, IPC_RMID, 0);
		exit(1);
	}

}

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

NumberPipe OuterPipes;

class Command {
public:
	vector<string> cmd;
	Command(vector<string>& vec): cmd(vec) {}

};


pid_t ForkProcess(vector<string> &cmd, vector<int> &currfd, OrdinaryPipe &Pipe) {
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
		user_pipes_clean_all();

		//if(execlp(command.c_str(), command.c_str(), argv[1], NULL) < 0) {
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
	if (broadcast_receive_msg != "" && broadcast_pipe_msg != "") {
		string total_message = broadcast_receive_msg + broadcast_pipe_msg;
		while(shm_buffer->lock);
		shm_buffer->lock = 1;
		strcpy(shm_buffer->broadcast_or_tell, total_message.c_str());
		for (int i = 1; i < 31; ++i) {
			if (clients[i].pid != 0) {
				kill(clients[i].pid, SIGUSR1);
			}
		}
		shm_buffer->lock = 0;
	} else if (broadcast_receive_msg != "") {
		while(shm_buffer->lock);
		shm_buffer->lock = 1;
		strcpy(shm_buffer->broadcast_or_tell, broadcast_receive_msg.c_str());
		for (int i = 1; i < 31; ++i) {
			if (clients[i].pid != 0) {
				kill(clients[i].pid, SIGUSR1);
			}
		}
		shm_buffer->lock = 0;
	} else if (broadcast_pipe_msg != "") {
		while(shm_buffer->lock);
		shm_buffer->lock = 1;
		strcpy(shm_buffer->broadcast_or_tell, broadcast_pipe_msg.c_str());
		for (int i = 1; i < 31; ++i) {
			if (clients[i].pid != 0) {
				kill(clients[i].pid, SIGUSR1);
			}
		}
		shm_buffer->lock = 0;
	}
	
	OrdinaryPipe Pipe;
	Pipe.pipe_cnt = cmd_list.size() - 1;
	int childpid = -1;
	int previous_childpid = -1;
	int writing_file_fd = -1;
	
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

		if (i == 0 && has_userpipe_receive) {
			if (sender_invalid) {
				currfd[0] = open("/dev/null", O_RDONLY);
			} else {
				currfd[0] = read_fd;
			}
		}

		if (i == cmd_list.size() - 1 && has_userpipe_send) {
			if (receiver_invalid) {
				currfd[1] = open("/dev/null", O_WRONLY);
			} else {
				currfd[1] = write_fd;
			}
		}
		
		childpid = ForkProcess(cmd_list[i].cmd, currfd, Pipe);

		Pipe.clean_before();             // must go first, close main pipe before wait
		Pipe.update();
		
		OuterPipes.close_used_pipes();

		if (i == 0 && has_userpipe_receive) {
			remove_userpipe(curr_sender_id, my_id);
			close(read_fd);
		}

		if (i == cmd_list.size() - 1 && has_userpipe_send) {
			close(write_fd);
		}
		
		
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

				if (s[0] == '!') {
					stderr_to_stdout = 1;
				}

				OuterPipes.create_outer_pipe(num);
				HandleOneLineCmd(one_line_cmd, 1);
				reset_flag();
				one_line_cmd.clear();
			}

			if (s[0] == '>') {
				OuterPipes.curr_line_no++;
				writing_file_mode = 1;
				ss >> writing_file_name;
				HandleOneLineCmd(one_line_cmd, 0);
				reset_flag();
				one_line_cmd.clear();
			}


		} else if (s[0] == '<' && s.size() != 1) {  // user pipe receive
			has_userpipe_receive = 1;
			int s_id;
			stringstream ss(s.substr(1));
			ss >> s_id;

			if (clients[s_id].pid == 0) {
				cout << "*** Error: user #" + to_string(s_id) + " does not exist yet. ***" << endl;
				sender_invalid = 1;
			} else {
				if (!all_user_pipes[s_id].receive[my_id]) {
					cout << "*** Error: the pipe #" + to_string(s_id) +"->#" + to_string(my_id) + " does not exist yet. ***" << endl;
					sender_invalid = 1;
				} else {
					curr_sender_id = s_id;
					int fd = all_user_pipes[s_id].receiver_open_fd[my_id];
					read_fd = fd;
					string my_name = clients[my_id].name;
					string sender_name = clients[s_id].name;
					//string broadcast_receive_msg = "*** " + my_name + " (#" + to_string(my_id) +") just received from " + sender_name + " (#" + to_string(s_id) + ") by '"+ str + "' ***\n";
					stringstream ss2(str);
					vector<string> full_cmd;
					string partial;
					broadcast_receive_msg = "*** " + my_name + " (#" + to_string(my_id) +") just received from " + sender_name + " (#" + to_string(s_id) + ") by '";
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
				}
			}
			

		} else if (s[0] == '>' && s.size() != 1) {  // user pipe send
			has_userpipe_send = 1;
			int r_id;
			stringstream ss(s.substr(1));
			ss >> r_id;

			// find in share memory
			if (clients[r_id].pid == 0) {
				cout << "*** Error: user #" + to_string(r_id) + " does not exist yet. ***" << endl;
				receiver_invalid = 1;
			} else {
				if (all_user_pipes[my_id].receive[r_id]) {
					cout << "*** Error: the pipe #" + to_string(my_id) + "->#" + to_string(r_id) + " already exists. ***" << endl;
					receiver_invalid = 1;
				} else {
					int r_pid = clients[r_id].pid;
					all_user_pipes[my_id].receive[r_id] = 1;
					// send signal first,
					kill(r_pid, SIGUSR2);
					string myfifo = "user_pipe/" + to_string(my_id) + "_to_" + to_string(r_id);
					mkfifo(myfifo.c_str(), 0666);
					// then open FIFO for write only
					int fd = open(myfifo.c_str(), O_WRONLY);
					write_fd = fd;
					all_user_pipes[my_id].sender_open_fd[r_id] = fd;

					string my_name = clients[my_id].name;
					string receiver_name = clients[r_id].name;
					//string broadcast_pipe_msg = "*** " + my_name + " (#" + to_string(my_id) +") just piped '" + str + "' to " + receiver_name + " (#" + to_string(r_id) + ") ***\n";
					stringstream ss2(str);
					vector<string> full_cmd;
					string partial;
					broadcast_pipe_msg = "*** " + my_name + " (#" + to_string(my_id) +") just piped '";
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
					broadcast_pipe_msg += ("' to " + receiver_name + " (#" + to_string(r_id) + ") ***\n");
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

}


void npshell(int id) {
	my_id = id;
	setenv("PATH", "bin:.", 1);
	
	//char* tmp = getenv("PATH");
	//cout << tmp << endl;

	while(1) {
		string str = "";
		cout << "% ";
		getline(cin, str);

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
			if (setenv(var.c_str(), value.c_str(), 1) < 0) {
				cout << "setenv error!" << endl;
			}
						
		} else if (s == "printenv") {
			// build-in command count as one line for numbered pipe
			OuterPipes.curr_line_no++;
			string var = "";
			ss >> var;
			char* tmp; 
			if (tmp = getenv(var.c_str())) {
				cout << tmp << endl;
			}
			
		} else if (s == "exit") {
			clients[my_id].pid = 0;
			string who_logout = clients[my_id].name;
			memset(clients[my_id].name, '\0', sizeof(clients[my_id].name));
			memset(clients[my_id].ip, '\0', sizeof(clients[my_id].ip));
			memset(clients[my_id].port, '\0', sizeof(clients[my_id].port));
			for (int i = 1; i < 31; ++i) {
				for (int j = 1; j < 31; ++j) {
					if (i == my_id || j == my_id) {
						all_user_pipes[i].receive[j] = 0;
						close(all_user_pipes[i].receiver_open_fd[j]);
						close(all_user_pipes[i].sender_open_fd[j]);
						all_user_pipes[i].receiver_open_fd[j] = -1;
						all_user_pipes[i].sender_open_fd[j] = -1;
						string myfifo = "user_pipe/" + to_string(i) + "_to_" + to_string(j);
						unlink(myfifo.c_str());
					}
				}
			}
			string logout_message = "*** User '" + who_logout + "' left. ***\n";
			while(shm_buffer->lock);
			shm_buffer->lock = 1;
			strcpy(shm_buffer->broadcast_or_tell, logout_message.c_str());
			for (int i = 1; i < 31; ++i) {
				if (clients[i].pid != 0) {
					kill(clients[i].pid, SIGUSR1);
				}
			}
			shm_buffer->lock = 0;

			break;
		} else if (s == "") {
		} else if (s == "who") {
			cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
			for (int i = 1; i < 31; ++i) {
				if (clients[i].pid != 0) {
					if (i == my_id) {
						cout << to_string(i) + "\t" + clients[i].name + "\t" + clients[i].ip + ":" + clients[i].port + "\t" + "<-me" << endl;
					} else {
						cout << to_string(i) + "\t" + clients[i].name + "\t" + clients[i].ip + ":" + clients[i].port + "\t" << endl;
					}
				}
			}

		} else if (s == "name") {
			string tmp_name;
			ss >> tmp_name;
			
			bool valid = 1;
			for (int i = 1; i < 31; ++i) {
				if (tmp_name == clients[i].name) {
					valid = 0;
					break;
				}
			}

			if (valid) {
				strcpy(clients[my_id].name, tmp_name.c_str());
				string ip = clients[my_id].ip;
				string port = clients[my_id].port;
				string message = ("*** User from " + ip + ":" + port + " is named '" + tmp_name + "'. ***\n");

				while(shm_buffer->lock);
				shm_buffer->lock = 1;
				strcpy(shm_buffer->broadcast_or_tell, message.c_str());
				for (int i = 1; i < 31; ++i) {
					if (clients[i].pid != 0) {
						kill(clients[i].pid, SIGUSR1);
					}
				}
				shm_buffer->lock = 0;

			} else {
				cout << "*** User '" + tmp_name + "' already exists. ***" << endl;
			}

		} else if (s == "tell") {
			int user_id;
			ss >> user_id;
			
			if (clients[user_id].pid != 0) {
				string tmp_name = clients[my_id].name;
				string message = "*** " + tmp_name + " told you ***: ";
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

				while(shm_buffer->lock);
				shm_buffer->lock = 1;
				strcpy(shm_buffer->broadcast_or_tell, message.c_str());
				kill(clients[user_id].pid, SIGUSR1);
				shm_buffer->lock = 0;

			} else {
				cout << "*** Error: user #" + to_string(user_id) + " does not exist yet. ***" << endl;
			}

		} else if (s == "yell") {
			string tmp_name = clients[my_id].name;
			string message = "*** " + tmp_name + " yelled ***: ";
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

			while(shm_buffer->lock);
			shm_buffer->lock = 1;
			strcpy(shm_buffer->broadcast_or_tell, message.c_str());
			for (int i = 1; i < 31; ++i) {
				if (clients[i].pid != 0) {
					kill(clients[i].pid, SIGUSR1);
				}
			}
			shm_buffer->lock = 0;

		} else {
			Parser(str);
		}
	}
}


int main(int argc, char *argv[], char *envp[]) {
	// create share memory
	shm_clients_id = shmget((key_t)12345, 31 * sizeof(struct client), 0644 | IPC_CREAT);
	shm_pipes_id = shmget((key_t)23456, 31 * sizeof(struct UserPipe), 0644 | IPC_CREAT);
	shm_buffer_id = shmget((key_t)34567, sizeof(buffer), 0644 | IPC_CREAT);
	all_user_pipes = (UserPipe*)shmat(shm_pipes_id, 0, 0);
	clients = (client*)shmat(shm_clients_id, 0, 0);
	shm_buffer = (buffer*)shmat(shm_buffer_id, 0, 0);

	for (int i = 0; i < 31; ++i) {
		clients[i].pid = 0;
		memset(clients[i].name, '\0', sizeof(clients[i].name));
		memset(clients[i].ip, '\0', sizeof(clients[i].ip));
		memset(clients[i].port, '\0', sizeof(clients[i].port));
	}

	for (int i = 0; i < 31; ++i) {
		for (int j = 0; j < 31; ++j) {
			all_user_pipes[i].receive[j] = 0;
			all_user_pipes[i].receiver_open_fd[j] = -1;
			all_user_pipes[i].sender_open_fd[j] = -1;
		}
	}

	memset(shm_buffer->broadcast_or_tell, '\0', sizeof(shm_buffer->broadcast_or_tell));
	shm_buffer->lock = 0;

	int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int port = atoi(argv[1]);
 
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

    signal(SIGCHLD, SIGHANDLE);
    signal(SIGINT, SIGHANDLE);
	signal(SIGUSR1, SIGHANDLE);
	signal(SIGUSR2, SIGHANDLE);

    while(1) {
    	if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
	        perror("accept failed");
	        exit(EXIT_FAILURE);
	    }

	    cout << "new connent fd: " << new_socket << endl;

	    int id;
    	for (id = 1; id < 31; ++id) {
    		if (clients[id].pid == 0) {
    			break;
    		}
    	}
	    
	    // child
	    int childpid = fork();
	    if (childpid == 0) {
			close(server_fd);
			close(0);
			close(1);
			close(2);
			dup(new_socket);
			dup(new_socket);
			dup(new_socket);
			cout << "****************************************\n** Welcome to the information server. **\n****************************************\n";
			string new_addr = inet_ntoa(address.sin_addr);
			string login_message = "*** User '(no name)' entered from " + new_addr + ":" + to_string(ntohs(address.sin_port)) + ". ***\n";

			while(shm_buffer->lock);
			shm_buffer->lock = 1;
			strcpy(shm_buffer->broadcast_or_tell, login_message.c_str());
			for (int i = 1; i < 31; ++i) {
				if (clients[i].pid != 0) {
					kill(clients[i].pid, SIGUSR1);
				}
			}
			shm_buffer->lock = 0;

			npshell(id);
			exit(0);
	    }
	    // parent
	    else {
			close(new_socket);
			clients[id].pid = childpid;
			cout << "id:" << id << "  pid:" << clients[id].pid << endl;
			strcpy(clients[id].name, "(no name)");
			strcpy(clients[id].ip, inet_ntoa(address.sin_addr));
			strcpy(clients[id].port, to_string(ntohs(address.sin_port)).c_str());
	    }


    }







return 0;	
}
































