#include <iostream>
#include <string>

#include <signal.h>
#include <string.h> // for memset

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SIZE 1024

/* write count bytes from buffer to fd, but handle
   partial writes (e.g. if the socket disconnects after
   some data is written)
 */
bool write_fully(int fd, const char *buffer, ssize_t count) {
    const char *ptr = buffer;
    const char *end = buffer + count;
    while (ptr != end) {
        ssize_t written = write(fd, (void*) ptr, end - ptr);
        if (written == -1) {
            return false;
        } 
        ptr += written;
    }
    return true;
}

void client_for_connection(int socket_fd) {
    while (1) {
        std::string line;
        std::getline(std::cin, line);
        if (!std::cin) {
            return;
        }

        line += '\n';
        
        if (false == write_fully(socket_fd, line.data(), line.size())) {
            std::cerr << "write: " << strerror(errno) << std::endl;
            return;
        }

        char buffer[1024];
        ssize_t buffer_used = 0;
        do {
            ssize_t count = read(socket_fd, buffer + buffer_used, sizeof(buffer) - buffer_used);
            if (count == -1) {
                std::cerr << "read: " << strerror(errno) << std::endl;
                return;
            } else if (count == 0) {
                // read end-of-file
                break;
            }
            buffer_used += count;
        } while (buffer_used != sizeof(buffer) && buffer[buffer_used - 1] != '\n');
        std::cout << "Read from server [" << std::string(buffer, buffer_used) << "]" << std::endl;
    }
}

int make_client_socket(const char *hostname, const char *portname) {
    struct addrinfo *server;
    struct addrinfo hints;
    int rv;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
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

int main(int argc, char **argv) {
    // ignore SIGPIPE, so writing to a socket the other end has closed causes
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
        std::cerr << "usage: " << argv[0] << " HOST PORT\n";
        std::cerr << "example: \n"
                  << argv[0] << " 127.0.0.1 9999\n";
        exit(1);
    }

    int connection_fd = make_client_socket(hostname, portname);
    client_for_connection(connection_fd);
    return 0;
}
