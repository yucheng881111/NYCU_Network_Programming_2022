#include <bits/stdc++.h>

using namespace std;

int main() {
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

	cout << "input cmd (b, c): ";
	string test_cmd;
	cin >> test_cmd;
	cout << "input ip: ";
	string test_ip;
	cin >> test_ip;

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
				cout << "pass" << endl;
				return 0;
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
				cout << "pass" << endl;
				return 0;
			}
		}
	}
	

	cout << "reject" << endl;
	return 0;
}













