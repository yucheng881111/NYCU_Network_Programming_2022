part1:
	g++ console.cpp -o console.cgi -std=c++17 -Wall -pedantic -lpthread -lboost_system
	g++ http_server.cpp -o http_server -std=c++17 -Wall -pedantic -lpthread -lboost_system
	g++ cgi_server.cpp -o cgi_server -std=c++17 -Wall -pedantic -lpthread -lboost_system
part2:
	g++ cgi_server_windows.cpp -o cgi_server.exe -lws2_32 -lwsock32 -std=c++17 -Wall -pedantic -lpthread -lboost_system -IC:\Users\yucheng\Downloads\MinGW\include
clean:
	rm console.cgi
	rm http_server
	rm cgi_server
