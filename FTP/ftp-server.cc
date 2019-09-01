#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <string.h> // for memset
#include <signal.h>
#include <dirent.h>
#include <vector>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SIZE 4096

//Stripping code adapted from stack overflow documentation
std::vector<std::string> format(char rb[], int read_count){
  std::string buff(rb);
  std::vector<std::string> command;
  int st=0;
  int e=0;
  while( buff[e]!=0x00 && e< read_count &&buff[e]!=' '&& buff[e]!= '\r'){
    e++;
  }
  command.push_back(buff.substr(st, e));
  st = e;
  st = st + 1;
  e=st;
 
  while(e<read_count&& buff[e]!='\n' ){
    while( buff[e]!='\r'&& buff[e]!=' '&& e<read_count && buff[e]!='\n' && buff[e]!=','){
      e++;
    }
    command.push_back(buff.substr(st, e-st));

    st = e;
	st = st + 1;
    e=st;
  }
    return command;
}


int make_client_socket(const char *hostname, const char *portname) {
    struct addrinfo *server;
    struct addrinfo hints;
    int rv;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    rv = getaddrinfo(hostname, portname, &hints, &server);
    if (rv != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }
    int fd = -1;
    for (struct addrinfo *addr = server; addr; addr = addr->ai_next) {
        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd == -1)
            continue;
        if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }
        std::cerr << "connect: " << strerror(errno) << std::endl;
        close(fd);
        fd = -1;
    }

    if (fd == -1) {
        std::cerr << "could not find address to connect to" << std::endl;
        return -1;
    }
   
    return fd;
}

/* write count bytes from buffer to fd, but handle
   partial writes (e.g. if the socket disconnects after
   some data is written)
 */
bool write_fully(int fd, const char *buffer, ssize_t count) {
    const char *ptr = buffer;
    const char *e = buffer + count;
    while (ptr != e) {
        ssize_t written = write(fd, (void*) ptr, e - ptr);
        if (written == -1) {
            return false;
        } 
        ptr += written;
    }
    return true;
}

bool write_to_fully(int fd, std::string message){
	//std::cout<<message<<" bupkis"<<std::endl;
	char * cstr = new char[message.size() + 1];
	strcpy(cstr, message.c_str());
	bool correct = false;
	correct = write_fully(fd, cstr, strlen(cstr));
	delete cstr;
	return correct;
}

void server_for_connection(int socket_fd) {
    int read_count;
	int port_number = -1;
	char type = 'A';
    char request_buf[MAX_SIZE];
	write_to_fully(socket_fd, "220 Service ready for new user \r\n");
    while (1) {
		//std::cout<<"I stED AGAIN"<<std::endl;
        /* read as much as possible into request_buf */
        read_count = read(socket_fd, (void*) request_buf, MAX_SIZE);
		/*std::cout<<"Here NOW!"<<std::endl;
		std::string commandLine = request_buf;
		std::string temp = "";
		std::stringstream ss(commandLine);
		std::vector<std::string> command; 
		while(getline(ss, temp, ' ')){
			std::cout<<temp<<std::endl;
			command.emplace_back(temp);
		}*/
		std::vector<std::string> command = format(request_buf, read_count);
		//std::cout<<"COMMAND O"<<command[0]<<std::endl;
        /* check for error, EOF */
        if (read_count == 0) {
            return; // EOF
        } else if (read_count == -1) {
            std::cerr << "read: " << strerror(errno) << std::endl;
            return;
        }
		if (command[0] == "USER"){
			//std::cout<<" I got here"<<std::endl;
			write_to_fully(socket_fd, "230 User logged in, proceed\r\n");
		} else if(command[0] == "ABOR" || command[0] == "CWD" || command[0] == "DendlE" || command[0] == "MDTM" ||
				  command[0] == "NLST" || command[0] == "PASS" || command[0] == "PASV" || command[0] == "PWD" ||
				  command[0] == "RMD" || command[0] == "RNFR" || command[0] == "RNTO" || command[0] == "SITE" || 
				  command[0] == "SIZE" || command[0] == "ACCT" || command[0] == "APPE" || command[0] == "CDUP" ||
				  command[0] == "HendlP" || command[0] == "REIN" || command[0] == "STAT" ||
				  command[0] == "STOU" || command[0] == "SYST"){
			write_to_fully(socket_fd, "502 Command Not Implemented \r\n");
		} else if(command[0] == "NOOP"){
			write_to_fully(socket_fd, "200 Command okay \r\n");
		} else if(command[0] == "MODE"){
			if(command[1] == "S"){
				write_to_fully(socket_fd, "200 Command okay \r\n");
			} else if(command[1] == "B" || command[1] == "C"){
				write_to_fully(socket_fd, "504 Command not implemented for that parameter \r\n");
			}
		} else if(command[0] == "TYPE"){
			if(command[1] == "A"){
				type = 'A';	
				write_to_fully(socket_fd, "200 Command okay \r\n");
			}else if(command[1] == "I"){
				type = 'I';
				write_to_fully(socket_fd, "200 Command okay \r\n");
			} else {
				write_to_fully(socket_fd, "504 Command not implemented for that parameter \r\n");			
			}
		} else if(command[0] == "QUIT"){
			write_to_fully(socket_fd, " 221 Service closing control connection. Logged out if appropriate. \r\n");
			return;
		} else if(command[0] == "STRU"){
			if(command[1] == "F"){
				write_to_fully(socket_fd, "200 Command okay \r\n");
			}else {
				write_to_fully(socket_fd, "504 Command not implemented for that parameter");
			}
		} else if(command[0] == "MKD"){
			int status = 0;
			status = mkdir(command[1].c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if(status == 0){
			write_to_fully(socket_fd, "257 " +command[1]+ " created \r\n");
			} else{
			write_to_fully(socket_fd, "501 Syntax error in parameters or arguments \r\n");
			}
		} else if(command[0] == "PORT"){
			if(port_number != -1){
				close(port_number);
			}
			std::string hostname = command[1] + "." + command[2] + "." + command[3] + "." + command[4];
			std::string portname = std::to_string((std::stoi(command[5]) * 256) + (std::stoi(command[6])));
			port_number = make_client_socket(hostname.c_str(), portname.c_str());
			std::cout<<port_number<< ": PORT NUMBER"<<std::endl;
			if( port_number != -1){
			write_to_fully(socket_fd, "200 Command okay \r\n");
			}else{
			port_number = -1;
			write_to_fully(socket_fd, "500 Syntax error in parameters \r\n");
			}
		} else if(command[0] == "STOR"){
			if(type != 'I' || port_number == -1){
			write_to_fully(socket_fd, "451 Requested action aborted: local error in processing \r\n");
			} else{
				//std::cout<<"I get to the beginning"<<std::endl;
				int destination_File = open(command[1].c_str(), O_CREAT|O_TRUNC|O_WRONLY, 0666);
				//std::cout<<"Port Number "<<port_number<<" File Descriptor "<<destination_File<<std::endl;
				if( destination_File < 0){
				write_to_fully(socket_fd, "450 Requested action not taken \r\n");
				}
				write_to_fully(socket_fd, "125 data connection ready \r\n");
				char buffer[4096];
				int d;
				while((d = recv(port_number, buffer, 4096, 0)) > 0){
					//std::cout<<"I got here"<<std::endl;
					int error = 0;
					error = write(destination_File, buffer, d);
					if (error < 0){
					break;
					}
					buffer[0] = '\0';
				}
				close(destination_File);
				write_to_fully(socket_fd, "250 Requested file action okay \r\n");
			}
		} else if(command[0] == "RETR" && type == 'I'){
			if(type != 'I' || port_number == -1){
			write_to_fully(socket_fd, "451 Requested action aborted: local error in processing \r\n");	
			} else{
				int destination_file = open(command[1].c_str(), O_RDONLY, 0666);
				if( destination_file < 0){
				write_to_fully(socket_fd, "450 Requested action not taken \r\n");
				}
				write_to_fully(socket_fd, "125 data connection ready \r\n");
				char buffer[4096];
				int d;
				while((d = read(destination_file, buffer, 4096)) > 0){
					//std::cout<<"I get to this part"<<std::endl;
					int error;
					error = send(destination_file, buffer, d, 0);
					if(error < 0){
					break;
					}
					buffer[0] = '\0';
				}
				close(destination_file);
				close(port_number);
				write_to_fully(socket_fd, "250 Requested file action okay \r\n");
			}
		} else if(command[0] == "LIST"){
			if((type != 'I' && type != 'A') || port_number == -1){
			write_to_fully(socket_fd, "451 Requested action aborted: local error in processing \r\n");	
			}
			
			DIR* dir;
			std::string theFiles = "";
			std::string root = ".";
			if(command.size() == 1){
			root = ".";
			} else if (command.size() == 2){
				if(command[1].at(0) == '.' && command[1].at(1) == '/'){
				root = command[1];
				}
				if(command[1].at(0) == '/'){
				root = "." + command[1];
				}
				else{
				root = "./" + command[1];
				}
			}
			dir = opendir(root.c_str());
			if(dir == NULL){
				write_to_fully(socket_fd, "550 requested action not taken. \r\n");
			}
			//std::cout<<"Sizey boi"<<command.size()<<std::endl;
			struct dirent* the_entry;
			while((the_entry = readdir(dir)) != NULL){
				theFiles = theFiles + std::string(the_entry->d_name) + "\n";
			}
			closedir(dir);
			write_to_fully(socket_fd, "125 data connection ready \r\n");
			send(port_number, theFiles.c_str(), theFiles.length(), 0);
			close(port_number);
			port_number = -1;
			write_to_fully(socket_fd, "250 Requested file action okay \r\n");
		}
		//std::cout <<"Type: "<<type<<std::endl;
        std::cout << "read [" << std::string(request_buf, read_count) << "]\n";
		request_buf[0] = '\0';
		//memset(request_buf, '\0', MAX_SIZE);
        /* write it back; note that we don't care whether we read whole lines, etc. */
     //   if (false == write_fully(socket_fd, request_buf, read_count)) {
     //       std::cerr << "write: " << strerror(errno) << std::endl;
     //       return;
     //   }
    }
}

int make_server_socket(const char *hostname, const char *portname) {
    struct addrinfo *server;
    struct addrinfo hints;
    int rv;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    rv = getaddrinfo(hostname, portname, &hints, &server);
    if (rv != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }
    int fd = -1;
    for (struct addrinfo *addr = server; addr; addr = addr->ai_next) {
        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd == -1)
            continue;
        /* make it so that we can bind() to the same address immediately
           after the server termiantes. By default, the address will be
           reserved for a while afterwards. */
        int one = 1;
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
            std::cerr << "setsockopt (SO_REUSEADDR): " << strerror(errno) << std::endl;
        if (bind(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
            // XXX: really should handle getaddrinfo() returning multiple names
            break;
        }
        close(fd);
        fd = -1;
    }

    if (fd == -1) {
        std::cerr << "could not find address to bind to" << std::endl;
        return -1;
    }
    

    /* st listening for connections */
    listen(fd, SOMAXCONN);

    freeaddrinfo(server);

    return fd;
}

void output_server_host(int server_fd) {
    struct sockaddr_storage my_addr;
    socklen_t my_addr_len = sizeof(my_addr);
    getsockname(server_fd, (struct sockaddr*) &my_addr, &my_addr_len);
    char host[1024], port[1024];
    int rv = getnameinfo(
        (struct sockaddr*) &my_addr, my_addr_len,
        host, sizeof(host),
        port, sizeof(port),
        NI_NUMERICSERV | NI_NUMERICHOST
    );
    if (rv == 0) {
        std::cout << "Server bound to host " << host << ", port " << port << std::endl;
        std::cout << "(0.0.0.0 or :: represent all possible host addresses)" << std::endl;
    } else {
        std::cerr << "getnameinfo: " << gai_strerror(rv) << std::endl;
    }
}

int main(int argc, char **argv) {
    // ignore SIGPIPE, so writing to a socket the other e has closed causes
    // write() to return an error rather than crashing the program
    signal(SIGPIPE, SIG_IGN);
    const char *portname = NULL;
    const char *hostname = NULL;
    if (argc == 2) {
        portname = argv[1];
    } else if (argc == 3) {
        hostname = argv[1];
        portname = argv[2];
    } else {
        std::cerr << "usage: " << argv[0] << " PORT\n";
        std::cerr << "   or: " << argv[0] << " HOST PORT\n";
        std::cerr << "example: \n"
                  << argv[0] << " 127.0.0.1 9999\n";
        exit(1);
    }

    int server_fd = make_server_socket(hostname, portname);
    output_server_host(server_fd);
    if (server_fd != -1) {
        while (1) {
            struct sockaddr_storage remote_addr;
            socklen_t remote_addr_len = sizeof(remote_addr);
            int connection_fd = accept(server_fd, (struct sockaddr*) &remote_addr, &remote_addr_len);
            if (connection_fd == -1) {
                std::cerr << "accept: " << strerror(errno) << std::endl;
                exit(1);
            }
            char remote_host[1024], remote_port[1024];
            int rv = getnameinfo(
                (struct sockaddr*) &remote_addr, remote_addr_len,
                remote_host, sizeof(remote_host),
                remote_port, sizeof(remote_port),
                NI_NUMERICSERV | NI_NUMERICHOST
            );
            if (rv == 0) {
                std::cout << "Accepted connection from " << remote_host << ":" << remote_port << std::endl;
            } else {
                std::cerr << "getnameinfo: " << gai_strerror(rv);
            }
            server_for_connection(connection_fd);
            close(connection_fd);
            std::cout << "Closed connection, waiting for next" << std::endl;
        }
    }
}