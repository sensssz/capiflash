#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <vector>

using std::string;
using std::unordered_map;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::fstream;
using std::ofstream;
using std::ios;

#define PORT 4242
#define BUF_SIZE 1024
#define GET_COM_LEN 4
#define PUT_COM_LEN 4
#define DEL_COM_LEN 4
#define SHUT_COM_LEN 5

#define READ_LOCK() pthread_rwlock_rdlock(&lock)
#define WRITE_LOCK() pthread_rwlock_wrlock(&lock)
#define UNLOCK() pthread_rwlock_unlock(&lock)

static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
static unordered_map<string, string> data;
static ofstream log_file;
static fstream data_file;
static bool quit = false;

static void error(const char *message) {
    perror(message);
    exit(1);
}

static void load_data() {
    data_file.open("data", fstream::in);
    string key;
    string value;
    while ((data_file >> key) && (data_file >> value)) {
        data[key] = value;
    }
    data_file.close();
    data_file.open("data", fstream::out | fstream::app);
}

static void save_data() {
    READ_LOCK();
    for (auto pair : data) {
        data_file << pair.first << ' ' << pair.second << endl;
    }
    UNLOCK();
    data_file.close();
}

// static void log(const char *message) {
// }

static void signal_handler(int signal) {
    if (signal == SIGINT) {
        quit = true;
    }
}

void split(char *command, vector<string> &res) {
    while (*command == ' ') {
        command++;
    }
    int start = 0;
    int end = 0;
    for (; command[end]; ++end) {
        if (command[end] == ' ') {
            int len = end - start;
            if (len > 0) {
                string part(command + start, len);
                res.push_back(part);
            }
            start = end + 1;
        }
    }
    if (start < end) {
        string part(command + start);
        res.push_back(part);
    }
}

static void do_get(int clientfd, char *com) {
    vector<string> parts;
    split(com, parts);
    if (parts.size() != 2) {
        write(clientfd, "Syntax Error", 13);
    } else {
        READ_LOCK();
        string value = data[parts[1]];
        UNLOCK();
        if (write(clientfd, value.c_str(), value.length() + 1) < 0) {
            cerr << "Error sending result" << endl;
        }
    }
}

static void do_put(int clientfd, char *com) {
    vector<string> parts;
    split(com, parts);
    if (parts.size() != 3) {
        write(clientfd, "Syntax Error", 13);
    } else {
        WRITE_LOCK();
        data[parts[1]] = parts[2];
        UNLOCK();
        if (write(clientfd, "Ack", 4) < 0) {
            cerr << "Error sending result" << endl;
        }
    }
}

static void do_delete(int clientfd, char *com) {
    vector<string> parts;
    split(com, parts);
    if (parts.size() != 2) {
        write(clientfd, "Syntax Error", 13);
    } else {
        auto it = data.find(parts[1]);
        if (it != data.end()) {
            data.erase(it);
        }
        if (write(clientfd, "Ack", 4)) {
            cerr << "Error sending result" << endl;
        }
    }
}

static void do_shut(int clientfd) {
    quit = true;
    write(clientfd, "Ack", 4);
    close(clientfd);
}

static void *client_thread(void *arg) {
    int clientfd = *((int *) arg);
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);
    char com_buf[BUF_SIZE];
    bzero(com_buf, BUF_SIZE);
    int bytes_read = 0;
    cout << "Client is " << clientfd << endl;
    while (!quit) {
        bytes_read = read(clientfd, com_buf, BUF_SIZE);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            } else {
                cerr << "ERROR reading from client" << endl;
                break;
            }
        }
        cout << "Command is " <<  com_buf << endl;
        if (strncmp("get ", com_buf, GET_COM_LEN) == 0) {
            do_get(clientfd, com_buf);
        } else if (strncmp("put ", com_buf, PUT_COM_LEN) == 0) {
            do_put(clientfd, com_buf);
        } else if (strncmp("del ", com_buf, DEL_COM_LEN) == 0) {
            do_delete(clientfd, com_buf);
        } else if (bytes_read == SHUT_COM_LEN &&
                   strncmp("shut", com_buf, SHUT_COM_LEN) == 0) {
            do_shut(clientfd);
        } else {
            write(clientfd, "Syntax Error", 13);
        }
    }
    close(clientfd);
    return nullptr;
}

static void wait_all(vector<pthread_t *> &threads) {
    for (auto thread : threads) {
        pthread_join(*thread, NULL);
    }
}

int main(int argc, char *argv[]) {
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        error("ERROR registering signal handler");
    }
    load_data();
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    vector<pthread_t *> threads;
    listen(sockfd, 5);
    cout << "Ready for connection" << endl;
    while (true) {
        if (quit) {
            wait_all(threads);
            break;
        }
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int clientfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (clientfd == EAGAIN || errno == EWOULDBLOCK) {
            usleep(1000);
            continue;
        } else if (clientfd < 0) {
            error("ERROR establishing connection with client");
        }
        cout << "Connection established. Client is " << clientfd << endl;
        pthread_t thread;
        pthread_create(&thread, NULL, client_thread, &clientfd);
        threads.push_back(&thread);
    }

    cout << "Cleaning up" << endl;
    save_data();
    if (close(sockfd) != 0) {
        cerr << "Error closing socket" << endl;
    }
    pthread_rwlock_destroy(&lock);

    return 0;
}