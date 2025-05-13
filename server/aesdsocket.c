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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define SUCCESS (0)
#define FAIL    (-1)

#define PORT_NUM ("9000")

#define BUFFER_SIZE (50)
#if (USE_AESD_CHAR_DEVICE == 1)
#define FILE_NAME ("/dev/aesdchar")
#else
#define FILE_NAME ("/var/tmp/aesdsocketdata")
#endif

#define SLEEP_TIME (10000000)

void *timestamp_thread(void *thread_param);
void *client_thread_function(void *thread_param);

struct thread_params_s
{
    int accept_fd;
    pthread_t thread_id;
    int thread_is_completed;
    pthread_mutex_t *file_mutex;
};


struct slist_data_s 
{
    struct thread_params_s thread_params; // thread params will hold thread ID and thread complete info
    SLIST_ENTRY(slist_data_s) entry;
};

struct timestamp_thread_param_s
{
    pthread_t thread_id;
    pthread_mutex_t *file_mutex;
    int thread_is_completed;
};



void signal_handler(int signal);
void *get_in_addr(struct sockaddr *sa);

static char caught_signal = 0;
static int socket_fd = 0;
int accept_fd = 0;
int app_result = SUCCESS;

static pthread_mutex_t file_mutex;
static SLIST_HEAD(slisthead, slist_data_s) head;
struct slist_data_s *queue_data_p = NULL;
struct timestamp_thread_param_s timestamp_thread_args;
static uint8_t client_thread_count = 0;

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
    syslog(LOG_INFO, "[ MP ] - aesdsocket app start... writing to: %s", FILE_NAME);

    int option;

    while ((option = getopt(argc, argv, "d")) != -1)
    {
        switch (option)
        {
        case 'd':
            deamon_mode = 1;
            syslog(LOG_INFO, "[ MP ] - aesdsocket app start with deamon mode");
            break;
        
        default:
            break;
        }
    }
    

    struct sigaction new_act;
    memset(&new_act, 0, sizeof(new_act));
    new_act.sa_handler = signal_handler;
    if(sigaction(SIGINT, &new_act, NULL) != 0) {
        syslog(LOG_ERR, "[ MP ] - sigaction - SIGINT Handler Def error");
        closelog();
        exit(FAIL);
    }
    if(sigaction(SIGTERM, &new_act, NULL) != 0) {
        syslog(LOG_ERR, "[ MP ] - sigaction - SIGTERM Handler Def error");
        closelog();
        exit(FAIL);
    }

    syslog(LOG_INFO, "[ MP ] - removing log file if it exists");
    remove(FILE_NAME);

    syslog(LOG_INFO, "[ MP ] - socket opening");

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(socket_fd == FAIL)
    {
        syslog(LOG_ERR, "[ MP ] - cannot open socket");
        perror("[ MP ] - Cannot Open Socket\n");
        closelog();
        close(socket_fd);
        exit(FAIL);
    }

    syslog(LOG_INFO, "[ MP ] - socket open");

    int yes = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
    {
        syslog(LOG_ERR,"[ MP ] - error setsockopt()");
        perror("[ MP ] - error setsockopt()\n");
        closelog();
        close(socket_fd);
    	exit(FAIL);
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = AI_PASSIVE;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(NULL, PORT_NUM, &hint, &res);
    if(rc != SUCCESS)
    {
        syslog(LOG_ERR, "[ MP ] - getaddrinfo error");
        perror("[ MP ] - Cannot get addr info\n");
        closelog();
        close(socket_fd);
        exit(FAIL);
    }
    rc = FAIL;
    syslog(LOG_INFO, "[ MP ] - socket binding");
    do
    {
        rc = bind(socket_fd, res->ai_addr, res->ai_addrlen);
    } while (rc != SUCCESS);
    freeaddrinfo(res);
    syslog(LOG_INFO, "[ MP ] - socket binded");

    if(deamon_mode)
    {
        syslog(LOG_INFO, "[ MP ] - in deamon mode");
        pid_t pid = fork();

        if(pid <= -1)
        {
            syslog(LOG_ERR, "[ MP ] - fork error");
            perror("[ MP ] - Cannot fork");
            closelog();
            close(socket_fd);
            exit(FAIL);
        }
        else if(pid > 0)
        {
            syslog(LOG_INFO, "[ MP ] - parent is closing");
            exit(SUCCESS);
        }
    }

    syslog(LOG_INFO, "[ MP ] - socket listening");
    rc = listen(socket_fd, 10);
    if(rc != SUCCESS)
    {
        syslog(LOG_ERR, "[ MP ] - listening error");
        perror("[ MP ] - Cannot listen\n");
        closelog();
        close(socket_fd);
        exit(FAIL);
    }

    SLIST_INIT(&head);

    rc = pthread_mutex_init(&file_mutex, NULL);
    if (rc != SUCCESS)
    {
        syslog(LOG_ERR, "[ MP ] - file mutex init error");
        perror("[ MP ] - Cannot init file mutex\n");
        closelog();
        exit(FAIL);
    }

    while(caught_signal == 0)
    {
        syslog(LOG_INFO, "[ MP ] - socket waits to accept connections");
        sin_size = sizeof(their_addr);
        accept_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (accept_fd == FAIL)
        {
            syslog(LOG_ERR, "[ MP ] - accepting error\n");
            perror("[ MP ] - Cannot accept\n");
        }
        else
        {
            if (!timestamp_thread_args.thread_id)
            {
                syslog(LOG_INFO, "[ MP ] - creating timestamp thread");
                timestamp_thread_args.file_mutex = &file_mutex;
                timestamp_thread_args.thread_is_completed = 0;
                rc = pthread_create(&(timestamp_thread_args.thread_id),
                                    NULL, // Use default attributes
                                    timestamp_thread,
                                    &timestamp_thread_args);
                if (rc != SUCCESS)
                {
                    syslog(LOG_ERR, "[ MP ] - cannot create timestamp thread");
                    perror("[ MP ] - cannot create timestamp thread\n");
                    closelog();
                    close(socket_fd);
                    exit(FAIL);
                }
            }
            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
            syslog(LOG_INFO, "[ MP ] - Accepted connection from %s\n", s);
            queue_data_p = malloc(sizeof(struct slist_data_s));
            if (queue_data_p == NULL)
            {
                syslog(LOG_ERR, "[ MP ] - cannot malloc queue_data_p for client thread");
                perror("[ MP ] - Cannot malloc queue_data_p for client thread");
                break;
            }
            queue_data_p->thread_params.accept_fd = accept_fd;
            queue_data_p->thread_params.file_mutex = &file_mutex;
            queue_data_p->thread_params.thread_is_completed = 0;

            SLIST_INSERT_HEAD(&head, queue_data_p, entry);

            rc = pthread_create(&(queue_data_p->thread_params.thread_id),
                                    NULL, // Use default attributes
                                    client_thread_function,
                                    queue_data_p);
            if (rc != SUCCESS)
            {
                syslog(LOG_ERR, "[ MP ] - client thread doesn't start");
                perror("[ MP ] - Cannot start client thread\n");
                queue_data_p->thread_params.thread_is_completed = 1;
                free(queue_data_p);
                break;
            }
            else
            {
                if(client_thread_count < 0xFF)
                    client_thread_count++;
                syslog(LOG_INFO, "[ MP ] - client thread started with TID : %lu | client thread count : %d",queue_data_p->thread_params.thread_id, client_thread_count);
            }
        }

        struct slist_data_s *datap = NULL;
        SLIST_FOREACH(datap, &head, entry)
        {
            if(datap->thread_params.thread_is_completed)
            {
                syslog(LOG_INFO, "[ MP ] - TID : %lu thread is finished... Joining...", datap->thread_params.thread_id);
                pthread_join(datap->thread_params.thread_id, NULL);
                syslog(LOG_INFO, "[ MP ] - TID : %lu thread joined", datap->thread_params.thread_id);
                SLIST_REMOVE(&head, datap, slist_data_s, entry);
                free(datap);
                if(client_thread_count)
                    client_thread_count--;
            }
        }
    }

    syslog(LOG_INFO, "[ MP ] - exited from loop, checking malloced data client_thread_count : %d", client_thread_count);
    while (client_thread_count)
    {
        struct slist_data_s *datap = NULL;
        SLIST_FOREACH(datap, &head, entry)
        {
            if(datap->thread_params.thread_is_completed)
            {
                syslog(LOG_INFO, "[ MP ] - TID : %lu thread is finished... Joining...", datap->thread_params.thread_id);
                pthread_join(datap->thread_params.thread_id, NULL);
                syslog(LOG_INFO, "[ MP ] - TID : %lu thread joined", datap->thread_params.thread_id);
                SLIST_REMOVE(&head, datap, slist_data_s, entry);
                free(datap);
                if(client_thread_count)
                    client_thread_count--;
            }
        }
    }

    syslog(LOG_INFO, "[ MP ] - waits timestamp thread end Timestamp TID : %lu", timestamp_thread_args.thread_id);
    while (!timestamp_thread_args.thread_is_completed);
    pthread_join(timestamp_thread_args.thread_id, NULL);

    syslog(LOG_INFO, "[ MP ] - caught signal = %d", caught_signal);
    syslog(LOG_INFO, "[ MP ] - Close socket fd");
    close(accept_fd);
    shutdown(socket_fd, 2);
    remove(FILE_NAME);
    closelog();
    exit(app_result);
}

void signal_handler(int signal)
{
    if(signal == SIGINT)
    {
        syslog(LOG_INFO, "[ SIG_H ] - SIGINT signal caught");
        caught_signal |= 0x01;
    }

    if(signal == SIGTERM)
    {
        syslog(LOG_INFO, "[ SIG_H ] - SIGTERM signal caught");
        caught_signal |= 0x02;
    }

    syslog(LOG_INFO, "[ SIG_H ] - caught signal = %d", caught_signal);

}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *client_thread_function(void *thread_param)
{
    struct slist_data_s* client_thread_params = thread_param;
    char buffer[BUFFER_SIZE];
    int read_finish = 0;
    FILE *fp;
    int rc = pthread_mutex_lock(client_thread_params->thread_params.file_mutex);
    if (rc != SUCCESS)
    {
        syslog(LOG_ERR, "[ CT ] - file_mutex_lock error in TID : %lu", client_thread_params->thread_params.thread_id);
        perror("[ CT ] - file_mutex_lock error\n");
        read_finish = 2;
    }
    else
    {
        if ((fp = fopen(FILE_NAME, "a+")) == NULL)
        {
            syslog(LOG_ERR, "[ CT ] - file opening error!\n");
            read_finish = 2;
        }
    }

    while(!read_finish)
    {
        memset(buffer, 0x00, BUFFER_SIZE);
        int bytes_read = recv(client_thread_params->thread_params.accept_fd, buffer, sizeof(buffer), 0);
        if(bytes_read == FAIL)
        {
            syslog(LOG_ERR, "[ CT ] - client_recv error");
            perror("[ CT ] - client_recv error\n");
            client_thread_params->thread_params.thread_is_completed = 3;
            read_finish = 3;
        }
        else if(bytes_read == 0)
        {
            syslog(LOG_INFO, "[ CT ] - No byte read... Connection Closed... from TID: %lu\n", client_thread_params->thread_params.thread_id);
            fclose(fp);
            read_finish = 4;
        }
        else
        {
            syslog(LOG_INFO, "[ CT ] - recv byte count = %d from TID : %lu", bytes_read, client_thread_params->thread_params.thread_id);
            if (memchr(buffer, '\n', bytes_read) != NULL)
            {
                syslog(LOG_INFO, "[ CT ] - end byte received from TID: %lu", client_thread_params->thread_params.thread_id);
                read_finish = 1;
            }
            fwrite(buffer, sizeof(char), bytes_read, fp);
        }
    }
    fclose(fp);

    if(read_finish == 1)
    {
        if ((fp = fopen(FILE_NAME, "r")) == NULL)
        {
            syslog(LOG_ERR, "[ CT ] - file opening for read error! from TID : %lu\n", client_thread_params->thread_params.thread_id);
            read_finish = 5;
        }
        else
        {
            while (!feof(fp))
            {
                memset(buffer, 0x00, BUFFER_SIZE);
                int bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
                if (bytes_read <= 0)
                {
                    syslog(LOG_INFO, "[ CT ] - file end reached");
                    break;
                }
                syslog(LOG_INFO, "[ CT ] - send byte count = %d from TID : %lu", bytes_read, client_thread_params->thread_params.thread_id);
                send(client_thread_params->thread_params.accept_fd, buffer, bytes_read, 0);
            }
        }
    }

    syslog(LOG_INFO,"[ CT ] - read from socket end | read_finish : %d from TID : %lu\n", read_finish, client_thread_params->thread_params.thread_id);
    close(client_thread_params->thread_params.accept_fd);
    fclose(fp);
    pthread_mutex_unlock(client_thread_params->thread_params.file_mutex);
    client_thread_params->thread_params.thread_is_completed = 1;
    return client_thread_params;
}

void *timestamp_thread(void *thread_param)
{
    struct timestamp_thread_param_s *timestamp_thread_params = thread_param;
    time_t t;
    struct tm *locale_time;
    const char timestamp[] = "timestamp:";
    char timestamp_string[200];
    char formatted_time[50];

    while(1)
    {
        usleep(SLEEP_TIME);
        if(caught_signal)
        {
            syslog(LOG_INFO, "[ TST ] - signal catched, exiting from TID : %lu", timestamp_thread_params->thread_id);
            break;
        }
        memset(timestamp_string, 0x00, 200);

        //Format timestamp
        t = time(NULL);
        locale_time = localtime(&t);
        strftime(formatted_time, sizeof(formatted_time), "%Y%m%d %H:%M:%S", locale_time);
        strcat(timestamp_string, timestamp);
        strcat(timestamp_string, formatted_time);
        strcat(timestamp_string, "\n");
        syslog(LOG_INFO, "[ TST ] - %s TID : %lu",timestamp_string, timestamp_thread_params->thread_id);

        // Write data to file
        FILE *fp;
        int rc = pthread_mutex_lock(timestamp_thread_params->file_mutex);
        if (rc != SUCCESS)
        {
            syslog(LOG_ERR, "file_mutex_lock error in TID : %lu", timestamp_thread_params->thread_id);
            perror("file_mutex_lock error\n");
            break;
        }
        else
        {
            if ((fp = fopen(FILE_NAME, "a+")) == NULL)
            {
                syslog(LOG_ERR, "file opening error!\n");
                perror("file opening error!\n");
            }
        }
        fwrite(timestamp_string, sizeof(char), sizeof(timestamp_string), fp);
        fclose(fp);
        pthread_mutex_unlock(timestamp_thread_params->file_mutex);
        syslog(LOG_INFO, "[ TST ] - timestamp info written to file");
    }
    syslog(LOG_INFO, "[ TST ] - thread completed TID : %lu", timestamp_thread_params->thread_id);
    timestamp_thread_params->thread_is_completed = 1;
    return timestamp_thread_params;
}

