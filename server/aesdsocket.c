// Server Side Socket Script
// Author: Sriram Rajkumar
// Reference -  https://beej.us/guide/bgnet/html/
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>

#define PORT "9000"
#define BACKLOG 20
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

// Global variables for signal handler
static volatile int keep_running = 1;
static int sockfd_global = -1;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Signal handler for SIGINT and SIGTERM
void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        
        // Close listening socket to unblock accept()
        if (sockfd_global != -1) {
            shutdown(sockfd_global, SHUT_RDWR);
        }
    }
}

// Setup signal handlers
void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        exit(1);
    }
}

int send_file_contents(int sockfd) {
    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL) {
        perror("fopen");
        syslog(LOG_ERR, "Failed to open data file for reading: %s", strerror(errno));
        return -1;
    }
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) == -1) {
            perror("send");
            syslog(LOG_ERR, "Failed to send data: %s", strerror(errno));
            fclose(fp);
            return -1;
        }
    }
    
    fclose(fp);
    return 0;
}

int handle_client_data(int new_fd) {
    char buffer[BUFFER_SIZE];
    char *packet = malloc(BUFFER_SIZE);
    size_t packet_len = 0;
    size_t packet_capacity = BUFFER_SIZE;
    ssize_t bytes_received;
    
    if (packet == NULL) {
        perror("malloc");
        syslog(LOG_ERR, "malloc failed: %s", strerror(errno));
        return -1;
    }
    
    while ((bytes_received = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        
        for (ssize_t i = 0; i < bytes_received; i++) {
            // Expand buffer if needed
            if (packet_len >= packet_capacity - 1) {
                packet_capacity *= 2;
                char *new_packet = realloc(packet, packet_capacity);
                if (new_packet == NULL) {
                    perror("realloc");
                    syslog(LOG_ERR, "realloc failed: %s", strerror(errno));
                    free(packet);
                    return -1;
                }
                packet = new_packet;
            }
            
            packet[packet_len++] = buffer[i];
            
            // Found complete packet (newline)
            if (buffer[i] == '\n') {
                packet[packet_len] = '\0';
                
                // Append to file
                FILE *fp = fopen(DATA_FILE, "a");
                if (fp == NULL) {
                    perror("fopen");
                    syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
                    free(packet);
                    return -1;
                }
                
                fputs(packet, fp);
                fclose(fp);
                
                // Send entire file contents back to client immediately
                if (send_file_contents(new_fd) == -1) {
                    syslog(LOG_ERR, "Failed to send file contents");
                    free(packet);
                    return -1;
                }
                
                // Reset for next packet
                packet_len = 0;
            }
        }
    }
    
    if (bytes_received == -1) {
        perror("recv");
        syslog(LOG_ERR, "recv failed: %s", strerror(errno));
        free(packet);
        return -1;
    }
    
    free(packet);
    return 0;
}

int main(int argc, char *argv[]){
    
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    int opt;
    int daemon = 0;
    int sockfd, new_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    struct sockaddr_storage client_addr;
    int rv;
    int yes = 1;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    // Setup signal handlers EARLY
    setup_signal_handlers();

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                closelog();
                return -1;
        }
    }

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rv));
        closelog();
        return -1;
    }

    // Try to create and bind socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            syslog(LOG_ERR, "setsockopt: %s", strerror(errno));
            close(sockfd);
            freeaddrinfo(servinfo);
            closelog();
            return -1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        syslog(LOG_ERR, "Failed to bind to port %s", PORT);
        closelog();
        return -1;
    }

    sockfd_global = sockfd;

    // Fork to daemon mode AFTER successful bind
    if (daemon) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            syslog(LOG_ERR, "fork: %s", strerror(errno));
            close(sockfd);
            closelog();
            return -1;
        }
        
        if (pid > 0) {
            // Parent process exits
            exit(0);
        }
        
        // Child process continues as daemon
        if (setsid() < 0) {
            perror("setsid");
            exit(1);
        }
        
        if (chdir("/") < 0) {
            perror("chdir");
            exit(1);
        }
        
        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // Redirect to /dev/null
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
        
        syslog(LOG_INFO, "aesdsocket daemon started");
    }

    // Start listening
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        syslog(LOG_ERR, "listen: %s", strerror(errno));
        close(sockfd);
        closelog();
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %s", PORT);

    // Main accept loop
    while(keep_running) {
        sin_size = sizeof(client_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        
        if (new_fd == -1) {
            if (errno == EINTR || !keep_running) {
                // Interrupted by signal
                break;
            }
            perror("accept");
            syslog(LOG_ERR, "accept: %s", strerror(errno));
            continue;
        }

        // Get client IP
        inet_ntop(client_addr.ss_family,
                  get_in_addr((struct sockaddr *)&client_addr),
                  s, sizeof(s));

        syslog(LOG_INFO, "Accepted connection from %s", s);

        // Handle client data
        if (handle_client_data(new_fd) == -1) {
            syslog(LOG_ERR, "handle_client_data failed for %s", s);
        }

        syslog(LOG_INFO, "Closed connection from %s", s);
        
        close(new_fd);
    }

    // Graceful shutdown
    syslog(LOG_INFO, "Shutting down gracefully");
    
    close(sockfd);
    
    // Delete data file
    if (unlink(DATA_FILE) == -1 && errno != ENOENT) {
        perror("unlink");
        syslog(LOG_ERR, "Failed to delete %s: %s", DATA_FILE, strerror(errno));
    } else {
        syslog(LOG_INFO, "Deleted data file %s", DATA_FILE);
    }
    
    closelog();
    return 0;
}