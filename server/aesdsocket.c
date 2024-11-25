#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <getopt.h>

#define SUCCESS (0)
#define FAIL    (-1)

#define PORT_NUM ("9000")

#define BUFFER_SIZE (255)
#define FILE_NAME ("/var/tmp/aesdsocketdata")

FILE *fp;

void signal_handler(int signal);
void *get_in_addr(struct sockaddr *sa);

char caught_signal = 0;
int sockfd;
int acceptfd;
int app_result = SUCCESS;

int main (int argc, char *argv[])
{
    int rc = 0;
    struct addrinfo hint;
    struct addrinfo *res;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    char deamon_mode = 0;

    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "aesdsocket app start");

    int option;

    while ((option = getopt(argc, argv, "d")) != -1)
    {
        switch (option)
        {
        case 'd':
            deamon_mode = 1;
            syslog(LOG_INFO, "aesdsocket app start with deamon mode");
            break;
        
        default:
            break;
        }
    }
    

    struct sigaction new_act;
    memset(&new_act, 0, sizeof(new_act));
    new_act.sa_handler = signal_handler;
    if(sigaction(SIGINT, &new_act, NULL) != 0) {
        syslog(LOG_ERR, "sigaction - SIGINT Handler Def error");
        closelog();
        exit(FAIL);
    }
    if(sigaction(SIGTERM, &new_act, NULL) != 0) {
        syslog(LOG_ERR, "sigaction - SIGTERM Handler Def error");
        closelog();
        exit(FAIL);
    }

    syslog(LOG_INFO, "socket opening");

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd == FAIL)
    {
        syslog(LOG_ERR, "cannot open socket");
        perror("Cannot Open Socket\n");
        closelog();
        close(sockfd);
        exit(FAIL);
    }

    syslog(LOG_INFO, "socket open");

    int yes=1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
    {
        syslog(LOG_ERR,"error setsockopt()");
        printf("error setsockopt()\n");
        closelog();
        close(sockfd);
    	exit(FAIL);
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = AI_PASSIVE;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(NULL, PORT_NUM, &hint, &res);
    if(rc != SUCCESS)
    {
        syslog(LOG_ERR, "getaddrinfo error");
        perror("Cannot get addr info\n");
        closelog();
        // freeaddrinfo(res);
        close(sockfd);
        exit(FAIL);
    }

    syslog(LOG_INFO, "socket binding");
    rc = bind(sockfd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if(rc != SUCCESS)
    {
        syslog(LOG_ERR, "binding error");
        perror("Cannot bind\n");
        closelog();
        close(sockfd);
        exit(FAIL);
    }
    syslog(LOG_INFO, "socket binded");

    if(deamon_mode)
    {
        syslog(LOG_INFO, "in deamon mode");
        pid_t pid = fork();

        if(pid <= -1)
        {
            syslog(LOG_ERR, "fork error");
            perror("Cannot fork");
            closelog();
            close(sockfd);
            exit(FAIL);
        }
        else if(pid > 0)
        {
            syslog(LOG_INFO, "parent is closing");
            exit(SUCCESS);
        }
    }

    syslog(LOG_INFO, "socket listening");
    rc = listen(sockfd, 10);
    if(rc != SUCCESS)
    {
        syslog(LOG_ERR, "listening error");
        perror("Cannot listen\n");
        closelog();
        close(sockfd);
        exit(FAIL);
    }

    while(caught_signal == 0)
    {
        syslog(LOG_INFO, "socket waits to accept connections");
        sin_size = sizeof(their_addr);
        acceptfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (acceptfd == FAIL)
        {
            syslog(LOG_ERR, "accepting error\n");
            perror("Cannot accept\n");
            closelog();
            app_result = FAIL;
        }

        if(app_result == FAIL) break;
        
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s\n", s);

        if ((fp = fopen(FILE_NAME, "a+")) == NULL)
		{
        	syslog(LOG_ERR,"file opening error!\n");
            closelog();
            app_result = FAIL;
            break;
		}
    	char buffer[BUFFER_SIZE];
        char end_byte = 0;
        while (end_byte == 0)
        {
            int bytes_read = recv(acceptfd, buffer, sizeof(buffer), 0);
            if(bytes_read == FAIL)
            {
                syslog(LOG_ERR, "error on recv");
                closelog();
                close(acceptfd);
                app_result = FAIL;
                break;
            }
            else if(bytes_read == 0)
            {
                end_byte = 1;
                syslog(LOG_INFO, "No byte read... Closed connection from %s\n", s);
            }
            else
            {
                syslog(LOG_INFO, "----> recv byte count = %d", bytes_read);
                if (memchr(buffer, '\n', bytes_read) != NULL)
                {
                    end_byte = 1;
                    syslog(LOG_ERR, "end byte received");
                }
                fwrite(buffer, sizeof(char), bytes_read, fp);
            }
        }
        fclose(fp);

        if(app_result == FAIL)
        {
            close(acceptfd);
            break;
        }


        if ((fp = fopen(FILE_NAME, "r")) == NULL)
		{
        	syslog(LOG_ERR,"file opening error!\n");
            closelog();
			// exit(FAIL);
            app_result = FAIL;
            break;
		}

        while(!feof(fp))
        {
            int bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
            if(bytes_read <= 0) break;
            syslog(LOG_INFO, "----> send byte count = %d", bytes_read);
            send(acceptfd, buffer, bytes_read, 0);
        }
        fclose(fp);

        syslog(LOG_INFO, "Close accepted socket fd");
        syslog(LOG_INFO, "Closed connection from %s\n", s);
        close(acceptfd);
    }

    syslog(LOG_INFO, "caught signal = %d", caught_signal);
    syslog(LOG_INFO, "Close socket fd");
    close(acceptfd);
    shutdown(sockfd, 2);
    remove(FILE_NAME);
    closelog();
    exit(app_result);
}

void signal_handler(int signal)
{
    if(signal == SIGINT)
    {
        syslog(LOG_INFO, "SIGINT signal caught");
        caught_signal |= 0x01;
    }

    if(signal == SIGTERM)
    {
        syslog(LOG_INFO, "SIGTERM signal caught");
        caught_signal |= 0x02;
    }

    syslog(LOG_INFO, "caught signal = %d", caught_signal);
    // syslog(LOG_INFO, "Close socket fd");
    // shutdown(sockfd, 2);
    // remove(FILE_NAME);
    // closelog();
    // exit(SUCCESS);

}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
