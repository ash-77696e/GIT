#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <netdb.h> 
#include <string.h>
#include "../structs.h"
#include "../IO.h"

int clientIds;
CommitNode* commitNodes;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

node* nullNode(node* tmp)
{
    tmp->status = malloc(sizeof(char) * 20);
    tmp->pathName = malloc(sizeof(char) * 20);
    tmp->versionNum = malloc(sizeof(char) * 20);
    tmp->hash = malloc(sizeof(char) * 20);
    tmp->next = NULL;
    return tmp;
}

int numDigits(int num)
{
    int count = 0;
    do
    {
        count++;
        num /= 10;
    } while (num != 0);

    return count;
}

node* manifest_to_LL(char* manifestContents)
{
    int index = 0;
    int manIndex = 0;
    char temp = '?';
    node* head = NULL;
    char* buffer = malloc(sizeof(char));
    int tabcount = 0;
    node* ptr;
    node* tmp = malloc(sizeof(node));
    tmp = nullNode(tmp);

    while(manifestContents[manIndex] != '\0')
    {
        char temp = manifestContents[manIndex];

        if(temp == '\n')
        {
            char* tmpstr = malloc(sizeof(char) * (index+2));
            bzero(tmpstr, index+2);
            memcpy(tmpstr, buffer, index+1);
            free(buffer);
            tmpstr[index + 1] = '\0'; // turns buffer into string
            if(head == NULL) // first node of linked list should be manifest version
            { 
                tmp = malloc(sizeof(node));
                tmp = nullNode(tmp);
                strcpy(tmp->versionNum, tmpstr);
                head = tmp;

                tmp = malloc(sizeof(node));
                tmp = nullNode(tmp);
            }
            else // add node to end of LL
            {
                strcpy(tmp->hash, tmpstr); // no tab after hash, only newline so we assign it to the node here
                ptr = head;
                while(ptr->next != NULL){
                    ptr = ptr->next;
                }
                ptr->next = tmp;
                tmp = malloc(sizeof(node));
                tmp = nullNode(tmp);
            }
            // reset buffer
            index = 0;
            tabcount = 0;
            buffer = malloc(sizeof(char));
            manIndex++;
            continue;
        }

        else if(temp == '\t') // add to tmpNode / set field of tmpNode
        {
            char* tmpstr = malloc(sizeof(char) * (index+2));
            bzero(tmpstr, index+2);
            memcpy(tmpstr, buffer, index+1);
            free(buffer);
            tmpstr[index + 1] = '\0'; // turns buffer into string
                    
            if(tabcount == 0) // status
            {
                strcpy(tmp->status, tmpstr);
            }  
            if(tabcount == 1) // pathname
            {
                strcpy(tmp->pathName, tmpstr);
            }
            if(tabcount == 2) // versionNum
            {
                strcpy(tmp->versionNum, tmpstr);
            }

            // reset buffer
            index = 0;
            buffer = malloc(sizeof(char));
            tabcount++;
            manIndex++;
            continue;
        }
        else // add to buffer if not tab or newline
        { 
            buffer[index] = temp;
            index++;
            char* tmpstr = malloc(sizeof(char) * (index + 1));
            bzero(tmpstr, index + 1);
            memcpy(tmpstr, buffer, index);
            free(buffer);
            buffer = tmpstr;
            manIndex++;
        }
        

    }
    return head;
}

char* readMessage(char* buffer, int fd)
{
    int status = 0;
    char temp = '\0';
    char* sizeStr = (char*) malloc(sizeof(char)*2);
    int index = 0;
    bzero(sizeStr, index+2);

    status = read(fd, &temp, 1);

    while(temp != ':')
    {
        sizeStr[index] = temp;
        index++;
        char* tmp = (char*) realloc(sizeStr, index+1);
        sizeStr = tmp;
        sizeStr[index] = '\0';
        status = read(fd, &temp, 1);
    }

    int size = atoi(sizeStr);
    
    char* msg = (char*) malloc(sizeof(char) * size+1);
    bzero(msg, size+1);
    read(fd, msg, size);
    return msg;
}

int sendMessage(char* cmd, int sockFD)
{
    int size = strlen(cmd);
    char* msg = (char*) malloc(sizeof(char) * (size + numDigits(size) + 2));
    bzero(msg, size + numDigits(size) + 2);
    sprintf(msg, "%d:", size);
    strcat(msg, cmd);
    write(sockFD, msg, strlen(msg));
}

void createFileLL(char* basePath, FileNode** fileRoot)
{
    DIR* dir = opendir(basePath);
    readdir(dir);
    readdir(dir);

    char path[20000];
    bzero(path, 20000);
    struct dirent* entry;

    while((entry = readdir(dir)) != NULL)
    {
        strcpy(path, basePath);
        if(path[strlen(path)-1] != '/')
            strcat(path, "/");
        strcat(path, entry->d_name);
        if(isDirectoryExists(path))
            createFileLL(path, fileRoot);
        else
        {
            if(fileRoot == NULL)
            {
                int fd = open(path, O_RDONLY);
                char* fileRootPath = (char*) malloc(sizeof(char) * (strlen(path) + 1));
                bzero(fileRootPath, strlen(path) + 1);
                strcpy(fileRootPath, path);
                (*fileRoot) = (FileNode*) malloc(sizeof(FileNode));
                (*fileRoot)->pathName = fileRootPath;
                (*fileRoot)->next = NULL;
                (*fileRoot)->contents = readFile((*fileRoot)->contents, fd);
                close(fd);
                (*fileRoot)->size = getFileSize(path);
            }
            else
            {
                int fd = open(path, O_RDONLY);
                char* fileRootPath = (char*) malloc(sizeof(char) * (strlen(path) + 1));
                bzero(fileRootPath, strlen(path) + 1);
                strcpy(fileRootPath, path);
                FileNode* node = (FileNode*) malloc(sizeof(FileNode));
                node->pathName = fileRootPath;
                node->contents = readFile(node->contents, fd);
                close(fd);
                node->size = getFileSize(path);
                node->next = *fileRoot;
                *fileRoot = node;           
            }
            
        }
        
    }

    closedir(dir);
}

CommitNode* addCommitNode(CommitNode* head, char* projName, int clientID, char* commit)
{
    if(head == NULL)
    {
        head = (CommitNode*) malloc(sizeof(CommitNode));
        head->commit = commit;
        head->clientID = clientID;
        head->projName = projName;
        head->next = NULL;
        return head;
    }

    CommitNode* node = (CommitNode*) malloc(sizeof(CommitNode));
    node->projName = projName;
    node->commit = commit;
    node->clientID = clientID;
    node->next = head;
    head = node;
    return head;
}

int create(char* token, int clientfd)
{
    token = &token[3];
    if(isDirectoryExists(token))
    {
        sendMessage("er:Directory exists on server", clientfd);
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
    sprintf(response, "sc");
    sendMessage(response, clientfd);

    free(manifestPath);
    return 0;
}

int destroy(char* token, int clientfd)
{
    token = &token[3];
    if(!isDirectoryExists(token))
    {
        sendMessage("er:Directory does not exist on server", clientfd);
        return -1;
    }

    char* destroyCmd = (char*) malloc(sizeof(char) * (strlen(token) + 20));
    bzero(destroyCmd, strlen(token) + 20);
    sprintf(destroyCmd, "rm -rf \"%s\"", token);
    system(destroyCmd);
    sendMessage("sc", clientfd);
}

int currentversion(char* token, int clientfd)
{
    token = &token[3];
    if(!isDirectoryExists(token))
    {
        sendMessage("er:Directory does not exist on server", clientfd);
        return -1;
    }
    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(token) + 13));
    bzero(manifestPath, strlen(token)+13);
    sprintf(manifestPath, "%s/.Manifest", token);
    
    int manifestFD = open(manifestPath, O_RDONLY);
    char* manifestContents = (char*)malloc(sizeof(char) * 20);
    manifestContents = readFile(manifestContents, manifestFD);
    node* head = manifest_to_LL(manifestContents);
    node* ptr = head;

    int len = 2;
    char* buffer = (char*) malloc(sizeof(char) * len);
    bzero(buffer, len);

    while(ptr != NULL)
    {
        if(ptr == head)
        {
            int vLen = strlen(ptr->versionNum) + 5;
            char* tmp = (char*) malloc(sizeof(char) * (len + vLen));
            bzero(tmp, len+vLen);
            memcpy(tmp, buffer, len);
            len += vLen;
            free(buffer);
            buffer = tmp;
            strcat(buffer, ptr->versionNum);
            strcat(buffer, "\n");
        }

        else if(ptr->pathName != NULL)
        {
            int pLen = strlen(ptr->pathName) + 5;
            char* tmp = (char*) malloc(sizeof(char) * (len + pLen));
            bzero(tmp, len+pLen);
            memcpy(tmp, buffer, len);
            len += pLen;
            free(buffer);
            buffer = tmp;
            strcat(buffer, ptr->pathName);
            strcat(buffer,"\t");
        
            int vLen = strlen(ptr->versionNum) + 5;
            tmp = (char*) malloc(sizeof(char) * (len + vLen));
            bzero(tmp, len+vLen);
            memcpy(tmp, buffer, len);
            len += vLen;
            free(buffer);
            buffer = tmp;
            printf("%s\n", ptr->versionNum);
            strcat(buffer, ptr->versionNum);
            strcat(buffer, "\n");
        }

        ptr = ptr->next;
    }

    char* message = (char*) malloc(sizeof(char) * (strlen(buffer) + 10));
    bzero(message, strlen(buffer) + 10);
    sprintf(message, "sc:%s", buffer);
    sendMessage(message, clientfd);

    close(manifestFD);
    free(buffer);
}

int checkout(char* token, int clientfd)
{
    token = &token[3];
    FileNode* root = NULL;
    createFileLL(token, &root);
    int numFiles = 0;
    int totalFileLength = 0;
    FileNode* ptr = root;
    
    while(ptr != NULL)
    {
        numFiles++;
        totalFileLength += (ptr->size + numDigits(ptr->size) + 5 + strlen(ptr->pathName) + numDigits(strlen(ptr->pathName)));
        ptr = ptr->next;
    }

    char* response = (char*) malloc(sizeof(char) * (totalFileLength + (numFiles * 5)));
    bzero(response, totalFileLength + (numFiles*5));
    sprintf(response, "sf:%d:", numFiles);
    int i;
    ptr = root;

    while(ptr != NULL)
    {
        char* pathLenStr = (char*) malloc(sizeof(char) * (numDigits(strlen(ptr->pathName)) + 5));
        bzero(pathLenStr, numDigits(strlen(ptr->pathName) + 5));
        sprintf(pathLenStr, "%d:", strlen(ptr->pathName));
        strcat(response, pathLenStr);
        strcat(response, ptr->pathName);
        strcat(response, ":");
        char* contentsLenStr = (char*) malloc(sizeof(char) * (numDigits(strlen(ptr->contents)) + 5));
        bzero(contentsLenStr, numDigits(strlen(ptr->contents)) + 5);
        sprintf(contentsLenStr, "%d:", strlen(ptr->contents));
        strcat(response, contentsLenStr);
        strcat(response, ptr->contents);
        strcat(response, ":");
        ptr = ptr->next;
    }
    
    printf("%s\n", response);
    sendMessage(response, clientfd);
    free(response);
}

int commit(char* token, int clientfd)
{
    token = &token[3];
    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(token) + 15));
    bzero(manifestPath, strlen(token)+15);
    sprintf(manifestPath, "%s/.Manifest", token);
    int manifestFD = open(manifestPath, O_RDONLY);
    char* manifestContents = readFile(manifestContents, manifestFD);
    close(manifestFD);
    char* message = (char*) malloc(sizeof(char) * (strlen(manifestContents) + 30));
    bzero(message, strlen(manifestContents) + 30);
    int id = clientIds++;
    sprintf(message, "sc:%d:%s", id, manifestContents);
    sendMessage(message, clientfd);
    char* clientResponse = readMessage(clientResponse, clientfd);
    commitNodes = addCommitNode(commitNodes, token, id, clientResponse);
    printf("%s\n", commitNodes->commit);
}

int socketStuff(int fd)
{ 
    char* buffer = readMessage(buffer, fd);
    char* tokens = strtok(buffer, ":");

    if(strcmp(tokens, "cr") == 0)
        create(buffer, fd);
    else if(strcmp(tokens, "de") == 0)
        destroy(buffer, fd);
    else if(strcmp(tokens, "cv") == 0)
        currentversion(buffer, fd);
    else if(strcmp(tokens, "co") == 0)
        checkout(buffer, fd);
    else if(strcmp(tokens, "cm") == 0)
        commit(buffer, fd);
    
    printf("Client: %d disconnected\n", fd);
    close(fd);

    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    clientIds = 0;
    commitNodes = NULL;

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