#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <vector>

using std::string;
using std::unordered_map;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::getline;
using std::vector;
using std::fstream;
using std::ofstream;
using std::ios;

#define PORT 4242
#define BUF_SIZE 1024
#define MYKV_COUT cout << "localhost> "
#define MYKV_ENDL endl << "localhost> " << flush

static void error(const char *message) {
    perror(message);
    exit(1);
}

static void prompt() {
    MYKV_COUT;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[BUF_SIZE];
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, 
          (char *) &serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(PORT);
    if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    bzero(buffer, BUF_SIZE);
    bool quit = false;
    prompt();
    while (!quit) {
        string line;
        getline(cin, line);
        if (line == "quit") {
            break;
        }
        if (write(sockfd, line.c_str(), line.length() + 1) < 0) {
            error("Error sending command");
        }
        if (read(sockfd, buffer, BUF_SIZE) < 0) {
            error("Error receiving response");
        }
        MYKV_COUT << buffer << MYKV_ENDL;
        if (line == "shut") {
            break;
        }
    }
    close(sockfd);

    return 0;
} 