#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int isDirectoryExists(const char* path)
{
    struct stat stats;
    stat(path, &stats);

    if(S_ISDIR(stats.st_mode))
        return 1;
    
    return 0;
}

char* non_blocking_read(char* buffer, int fd)
{
    printf("here");
    //int bufferSize = 2;
    int bufferSize = 100;
    //buffer = (char*) malloc(sizeof(char) * bufferSize);
    bzero(buffer, bufferSize);

    int status = 1;
    int readIn = 0;

    do
    {
        status = read(fd, buffer+readIn, bufferSize-readIn);
        readIn += status;
        if(status == 0)
            break;
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

int create(char* token, int clientfd)
{
    token = strtok(NULL, ":");
    if(isDirectoryExists(token))
    {
        write(clientfd, "er:Directory exists on server", 29);
        return -1;
    }
    mkdir(token, 00777);
    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(token) + 12));
    bzero(manifestPath, strlen(token)+12);
    sprintf(manifestPath, "%s/.Manifest", token);
    printf("%s\n", manifestPath);
    int fd = open(manifestPath, O_RDWR | O_CREAT | O_TRUNC, 00600);
    write(fd, "0\n", 2);
    close(fd);
    char response[50];
    bzero(response, 50);
    sprintf(response, "sf:1:%s/.Manifest:0\n\0", token);
    write(clientfd, response, strlen(response));
    free(manifestPath);
    return 0;
}

int destroy(char* token, int clientfd)
{
    token = strtok(NULL, ":");
    if(!isDirectoryExists(token))
    {
        write(clientfd, "er:Directory does not exist on server", 38);
        return -1;
    }
    char* destroyCmd = (char*) malloc(sizeof(char) * (strlen(token) + 20));
    bzero(destroyCmd, strlen(token) + 20);
    sprintf(destroyCmd, "rm -rf \"%s\"", token);
    printf("%s\n", destroyCmd);
    system(destroyCmd);
    write(clientfd, "sc", 2);
}

int socketStuff(int fd)
{ 
    char* buffer = (char*) malloc(sizeof(char) * 100);
    read(fd, buffer, 50);

    char* tokens = strtok(buffer, ":");
    if(strcmp(tokens, "cr") == 0)
        create(tokens, fd);
    else if(strcmp(tokens, "de") == 0)
        destroy(tokens, fd);
    
    printf("Client: %d disconnected\n", fd);
    close(fd);

    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if(argc < 2) 
    {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) 
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    while(1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
          error("ERROR on accept");
        else
        {
            printf("Connected to Client: %d\n", newsockfd);
            socketStuff(newsockfd);
        }
    } 
     
     return 0; 
}