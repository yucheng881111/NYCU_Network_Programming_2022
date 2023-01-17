#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

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
bool writing_file_mode = 0;
string writing_file_name = "";
bool stderr_to_stdout = 0;

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
		
		childpid = ForkProcess(cmd_list[i].cmd, currfd, Pipe);

		Pipe.clean_before();             // must go first, close main pipe before wait
		Pipe.update();
		//cout << childpid << endl;
		
		OuterPipes.close_used_pipes();
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

void test() {
	vector<Command> cmd_list;
	// removetag test.html | number
	vector<string> v1 = {"removetag", "test.html"};
	vector<string> v2 = {"number"};
	// ls | cat | number
	vector<string> v3 = {"ls"};
	vector<string> v4 = {"cat"};
	vector<string> v5 = {"number"};
	Command c1(v1);
	Command c2(v2);
	Command c3(v3);
	Command c4(v4);
	Command c5(v5);
	
	cmd_list.push_back(c1);
	cmd_list.push_back(c2);
	
	
	cmd_list.clear();
	cmd_list.push_back(c3);
	cmd_list.push_back(c4);
	cmd_list.push_back(c5);
	
	HandleOneLineCmd(cmd_list, 0);
	
}

void test_outer() {
	// removetag test.html |3
	// removetag test.html |2
	// ls | cat | number
	// number

	vector<string> v1 = {"removetag", "test.html"};
	vector<string> v2 = {"number"};
	vector<string> v3 = {"ls"};
	vector<string> v4 = {"cat"};
	Command c1(v1);
	Command c2(v2);
	Command c3(v3);
	Command c4(v4);

	vector<Command> cmd_list;

	// removetag test.html |3
	OuterPipes.curr_line_no++;
	OuterPipes.create_outer_pipe(3);
	cmd_list.push_back(c1);
	HandleOneLineCmd(cmd_list, 1);
	cmd_list.clear();

	// removetag test.html |2
	OuterPipes.curr_line_no++;
	OuterPipes.create_outer_pipe(2);
	cmd_list.push_back(c1);
	HandleOneLineCmd(cmd_list, 1);
	cmd_list.clear();

	// ls | cat | number
	OuterPipes.curr_line_no++;
	//OuterPipes.create_outer_pipe(1);
	//cmd_list.push_back(c2);
	cmd_list.push_back(c3);
	cmd_list.push_back(c4);
	cmd_list.push_back(c2);

	HandleOneLineCmd(cmd_list, 0);
	cmd_list.clear();
	
	// number
	OuterPipes.curr_line_no++;
	//OuterPipes.create_outer_pipe(1);
	cmd_list.push_back(c2);
	HandleOneLineCmd(cmd_list, 0);
	cmd_list.clear();
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
		if (s[0] == '|' || s[0] == '!' || s[0] == '>') {
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
				stderr_to_stdout = 0;
				one_line_cmd.clear();
			}

			if (s[0] == '>') {
				OuterPipes.curr_line_no++;
				writing_file_mode = 1;
				ss >> writing_file_name;
				HandleOneLineCmd(one_line_cmd, 0);
				writing_file_mode = 0;
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
	}
	


}


int main(int argc, char *argv[], char *envp[]) {
	
	setenv("PATH", "bin:.", 1);
	
	//char* tmp = getenv("PATH");
	//cout << tmp << endl;

	//test();
	//test_outer();

	while(1) {
		string str = "";
		cout << "% ";
		getline(cin, str);
		if (str == "") {
			continue;
		}
		
		if (str == "exit") {
			break;
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
			
		} else {
			Parser(str);
		}
	}



return 0;
}





