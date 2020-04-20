#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}

char* non_blocking_read(char* buffer, int fd)
{
    int bufferSize = 256;
    buffer = (char*) malloc(sizeof(char) * bufferSize);
    bzero(buffer, bufferSize);

    int status = 1;
    int readIn = 0;

    do
    {
        status = read(fd, buffer+readIn, 1);
        readIn += status;
        
        if(readIn >= bufferSize)
        {
            char* tmp = (char*) malloc(sizeof(char) * bufferSize*2);
            bzero(tmp, bufferSize*2);
            memcpy(tmp, buffer, bufferSize);
            free(buffer);
            buffer = tmp;
            bufferSize *= 2;
        }
    } while (status > 0);
    
    return buffer;
}

int create_socket()
{
    int sockfd, n, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char* IP = (char*) malloc(sizeof(char) * 100);
    bzero(IP, 100);

    get_configure(IP, &portno);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(IP);
    if (server == NULL)
        error("ERROR no such host");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    int connectVal;

    do
    {
        connectVal = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if(connectVal != 0)
        {
            printf("Connection failed. Retrying in 3 seconds\n");
            sleep(3);
        }
    } while (connectVal != 0);
    
    printf("Connection successful!\n");
    
    free(IP);
    
    return sockfd;
}

int set_configure(char* IP, char* port)
{
    int fd = open("./.configure", O_RDWR | O_CREAT | O_TRUNC, 00600);
    write(fd, IP, strlen(IP));
    write(fd, " ", 1);
    write(fd, port, strlen(port));
    close(fd);
}

int get_configure(char* IP, int* port)
{
    int fd = open("./.configure", O_RDONLY);
    char buffer[500];
    bzero(buffer, 500);
    read(fd, &buffer, 500);
    char* token = strtok(buffer, " ");
    memcpy(IP, token, strlen(token));
    token = strtok(NULL, " ");
    *port = atoi(token);
    close(fd);
}

int main(int argc, char* argv[])
{
    
    if(argc > 1) 
    {
        if(strcmp(argv[1], "configure") == 0)
        {
            set_configure(argv[2], argv[3]);
        }

        if(strcmp(argv[1], "create") == 0)
        {
            mkdir(argv[2], 00777);
            int sockFD = create_socket();
            char* createCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(createCommand, strlen(argv[2])+4);
            sprintf(createCommand, "cr:%s", argv[2]);
            write(sockFD, createCommand, strlen(createCommand));
            char* serverResponse = (char*) malloc(sizeof(char) * 1000);
            read(sockFD, serverResponse, 1000);
            char* tokens = strtok(serverResponse, ":");
            if(strcmp(tokens, "sf") == 0)
            {
                tokens = strtok(NULL, ":");
                int numFiles = atoi(tokens);
                tokens = strtok(NULL, ":");
                int fd = open(tokens, O_RDWR | O_CREAT, 00600);
                tokens = strtok(NULL, ":");
                write(fd, tokens, strlen(tokens));
                printf("Create command completed\n");
            }
            else if(strcmp(tokens, "er") == 0)
            {
                tokens = strtok(NULL, ":");
                printf("Error: %s\n", tokens);
            }

            free(createCommand);
            free(serverResponse);
            close(sockFD);
        }

        if(strcmp(argv[1], "destroy") == 0)
        {
            int sockFD = create_socket();
            char* deleteCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(deleteCommand, strlen(argv[2]) + 4);
            sprintf(deleteCommand, "de:%s", argv[2]);
            write(sockFD, deleteCommand, strlen(deleteCommand));
            char* serverResponse = (char*) malloc(sizeof(char) * 1000);
            read(sockFD, serverResponse, 1000);
            char* tokens = strtok(serverResponse, ":");
            if(strcmp(tokens, "sc") == 0)
            {
                printf("Destroy command completed\n");
            }
            else if(strcmp(tokens, "er") == 0)
            {
                tokens = strtok(NULL, ":");
                printf("Error: %s\n", tokens);
            }

            free(deleteCommand);
            free(serverResponse);
            close(sockFD);
        }
    }

    return 0;
}