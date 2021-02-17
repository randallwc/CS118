#include <iostream> // cout // cerr
#include <sys/time.h> // timeval
#include <regex> // regex
#include <sstream> // stringstream
#include <netdb.h> // addrinfo
#include <unistd.h> // getopt // close // read // optarg
using namespace std;

// #include <sys/socket.h> // socket
// #include <sys/select.h> // select
// #include <cstring> // string
// #include <arpa/inet.h> // ...
// #include <netinet/in.h> // getaddrinfo

#define BUFFER_SIZE 1024

int main(int argc, char ** argv){
	//FLAGS
	bool a_flag = false;
	bool c_flag = false;

	//command line arguments
	string url = "";

	int ch;
	while( (ch = getopt(argc, argv, "a:c")) != -1){
		switch(ch){
			case 'a':
				a_flag = true;
				url = optarg;
				break;
			case 'c':
				c_flag = true;
				break;
			default:
				break;
		}
	}

	// check if url is given
	if(a_flag == false){
		cerr << "ERROR:\tno url given\n";
		return 2;
	}

	// cerr << "url: " << url << endl;

	// get the url with regex
	// regex e ("^http:\\/\\/([A-Za-z0-9-_\\.]+)(\\/?.*)");
	regex e ("^http://([^/]+)(/?.*)");
	smatch sm;
	if (regex_match(url,sm,e) == 0){
		cerr << "ERROR:\tinvalid url given [" << url << "]\n";
		return 1;
	}

	// parse the hostname and pathname from the regex
	string hostname = sm[1];
	string pathname = sm[2];

	// check if pathname exists
	if (pathname.length() == 0){
		pathname = "/";
	}

	// cerr << "hostname: " << hostname << endl;
	// cerr << "pathname: " << pathname << endl;

	// format the get request
	stringstream getrequest;
	getrequest << "GET " << pathname 
	<< " HTTP/1.1\r\nHost: " << hostname 
	<< "\r\nUser-Agent: http-get 1.0\r\n\r\n";

	// if c option is given then output to stdout
	if(c_flag){
		cout << getrequest.str();
		return 0;
	}

	// get the IP address
	struct addrinfo address_info;
	struct addrinfo *res;
	memset(&address_info, 0, sizeof(address_info));
	address_info.ai_family = AF_INET;
	address_info.ai_socktype = SOCK_STREAM;
	address_info.ai_flags = 0;
	address_info.ai_protocol = 0;

	if ((getaddrinfo(hostname.c_str(), "80", &address_info, &res)) != 0)
	{
		cerr << "ERROR:\tip cannot be obtained from url [" << url << "]\n";
		return 1;
	}

	// http://www.cplusplus.com/forum/articles/9742/
	// cerr << "IP: " << inet_ntoa(((struct sockaddr_in *)res->ai_addr)->sin_addr) << endl;

	// make the socket
	int sock = socket(address_info.ai_family, address_info.ai_socktype, 0);
	if (sock < 0) {
		cerr << "ERROR:\tsocket cannot be created\n";
		return 1;
	}

	// connect to the socket
	if (connect(sock, res->ai_addr, res->ai_addrlen) < 0){
		cerr << "ERROR:\tconnect failed\n";
		return 1;
	}

	// send get request
	send(sock, (void *) getrequest.str().c_str(), (size_t) getrequest.str().length(), 0);

	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	while(true){
		fd_set readfds;
		FD_SET(sock, &readfds);
		if (select(sock + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
			int size = read(sock, buffer, BUFFER_SIZE-1);
			if (size == 0){
				break;
			}
			else if (size < 0){
				cerr << "ERROR: read failed" << endl;
				return 1;
			}
			buffer[size] = '\0';
			fprintf(stdout, "%s", buffer);
		}
		else {
			break;
		}
	}

	close(sock);

	return 0;
}



