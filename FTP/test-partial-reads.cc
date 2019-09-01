#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

#include <signal.h>
#include <string.h> // for memset

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

bool write_fully(int fd, const std::string &str) {
    return write_fully(fd, str.data(), str.size());
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

bool read_and_discard_reply(int fd) {
    int newline_offset = 0;
    bool maybe_number = false;
    bool found_last_line = false;
    while (true) {
        char buffer[1024];
        int count = read(fd, buffer, sizeof buffer);
        if (count == -1)
            std::cerr << "read: " << strerror(errno);
        if (count <= 0)
            return false;
        for (int i = 0; i < count; ++i) {
            ++newline_offset;
            if (newline_offset == 1) {
                maybe_number = (buffer[i] >= '0' && buffer[i] <= '9');
            } else if (newline_offset == 4 && maybe_number && buffer[i] == ' ') {
                found_last_line = true;
            } else if (buffer[i] == '\n') {
                newline_offset = 0;
            }
        }
        if (newline_offset == 0 && found_last_line)
            break;
    }
    return true;
}

void client_for_connection(int socket_fd) {
    if (!read_and_discard_reply(socket_fd)) return;
    if (!write_fully(socket_fd, "USER foo\r\n")) return;
    if (!read_and_discard_reply(socket_fd)) return;
    if (!write_fully(socket_fd, "MKD testSplit")) return;
    sleep(1);
    if (!write_fully(socket_fd, "Name\r\n")) return;
    if (!read_and_discard_reply(socket_fd)) return;
    if (!write_fully(socket_fd, "QUIT\r\n")) return;
}

int main(int argc, char **argv) {
    // ignore SIGPIPE, so writing to a socket the other end has closed causes
    // write() to return an error rather than crashing the program
    signal(SIGPIPE, SIG_IGN);
    const char *hostname = "127.0.0.1";
    const char *portname = NULL;
    if (argc == 2) {
        portname = argv[1];
    } else {
        std::cerr << "usage: " << argv[0] << " PORT\n";
        std::cerr << "example: \n"
                  << argv[0] << " 9999\n";
        exit(1);
    }

    int connection_fd = make_client_socket(hostname, portname);
    if (connection_fd == -1) return 1;
    client_for_connection(connection_fd);
    close(connection_fd);
    return 0;
}
