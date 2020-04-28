#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <openssl/md5.h>
#include <errno.h>
#include "../structs.h"
#include "../IO.h"

void error(char *msg)
{
    perror(msg);
    exit(0);
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

node* nullNode(node* tmp)
{
    tmp->status = malloc(sizeof(char) * 20);
    tmp->pathName = malloc(sizeof(char) * 20);
    tmp->versionNum = malloc(sizeof(char) * 20);
    tmp->hash = malloc(sizeof(char) * 20);
    tmp->next = NULL;
    return tmp;
}

char* manifestLine(char* string, node* ptr)
{
    strcpy(string, ptr->status);
    strcat(string, "\t");
    strcat(string, ptr->pathName);
    strcat(string, "\t");
    strcat(string, ptr->versionNum);
    strcat(string, "\t");
    strcat(string, ptr->hash);
    strcat(string, "\n"); 
    return string;
}

char* manifestPath(char* projectName)
{
    char* result = malloc(sizeof(char) * ( strlen(projectName) + 11));
    bzero(result, strlen(projectName) + 11);
    memcpy(result, projectName, strlen(projectName));
    strcat(result, "/");
    strcat(result, ".Manifest");
    return result;
}

char* filePath(char* projectName, char* fileName)
{
    char* result = malloc(sizeof(char) * ( strlen(projectName) + strlen(fileName) + 1));
    bzero(result, strlen(projectName) + strlen(fileName) + 1 );
    memcpy(result, projectName, strlen(projectName));
    strcat(result, "/");
    strcat(result, fileName);
    return result; 
}

node* manifest_to_LL(int fd)
{
    int status;
    int index = 0;
    char temp = '?';
    node* head = NULL;
    char* buffer = malloc(sizeof(char));
    int tabcount = 0;
    node* ptr;
    node* tmp = malloc(sizeof(node));
    tmp = nullNode(tmp);

    do
    {
        status = read(fd, &temp, sizeof(char)); 

        if(status > 0)
        {
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
            }
        }

    } while(status > 0);
    return head;
}

void LL_to_manifest(node* head, int fd)
{
    node* ptr = head;
    while(ptr != NULL)
    {
        char* string = malloc(sizeof(char) * 100);
        bzero(string, 100);

        if(ptr == head) // write manifest version and newline
        {
            strcpy(string, ptr->versionNum);
            strcat(string, "\n");
            write(fd, string, strlen(string));
        }
        else
        {
            strcpy(string, manifestLine(string, ptr));
            write(fd, string, strlen(string));    
        }
        ptr = ptr->next;
    }
}

int add(char* projectName, char* fileName) // returns 1 on success 0 on failure
{ 
    if(!isDirectoryExists(projectName))
    {
        return 0; // project does not exist on client side
    }
    else{
        char* temp_file_name = malloc(sizeof(char) * 50);
        strcpy(temp_file_name, fileName);
        char* temp_project_name = malloc(sizeof(char) * 50);
        strcpy(temp_project_name, projectName);
        int fd = open(manifestPath(projectName), O_RDONLY);

        if(fd == -1)
        {
            printf("Manifest does not exist\n");
            return 0;
        }  

        node* head = manifest_to_LL(fd);
        

        // go through LL with fileName as key
        strcpy(fileName, temp_file_name);
        node* ptr = head;
        int modify = 0;
        while(ptr != NULL){
            if(strcmp(ptr->pathName, filePath(temp_project_name, fileName)) == 0) // match
            {
                strcpy(ptr->status, "M"); // modify
                printf("File already in manifest\n");
                modify = 1; // did not add
                break; 
            }
            ptr = ptr->next;

        }  

        if(ptr == NULL) // reached end of LL without matching
        {
            ptr = head;
            while(ptr->next != NULL){
                ptr = ptr->next;
            }
            
            int fd2 = open(filePath(temp_project_name, fileName), O_RDONLY);
            if(fd2 == -1)
            {
                printf("File does not exist\n");
                return 0;
            }
            char* fileContents = NULL;
            fileContents = readFile(fileContents, fd2);
            
            /*md5 hash fileContents, make this hash into a string, 
            then make a new node with status A, filename as pathname,
            version as 0, and hash as the md5 hash string*/
            
            unsigned char* result = malloc(sizeof(char) * strlen(fileContents));
            char* hashedStr = (char*) malloc(sizeof(char) * 33);
            bzero(hashedStr, 33);

            MD5_CTX context;
            MD5_Init(&context);
            MD5_Update(&context, fileContents, strlen(fileContents));
            MD5_Final(result, &context);
            int i;
            unsigned char resultstr[32];
            for (i=0; i<16; i++) {
                sprintf(resultstr, "%02x", result[i]);
                strcat(hashedStr, resultstr);
            }

            node* resultNode = malloc(sizeof(node));
            resultNode = nullNode(resultNode);
            strcpy(resultNode->status, "A"); // add
            strcpy(resultNode->pathName, filePath(temp_project_name, fileName));
            strcpy(resultNode->versionNum, "0");
            strcpy(resultNode->hash, hashedStr);

            ptr->next = resultNode; // adds node that represents last line in manifest

            close(fd2);
            
        }

        close(fd);
        
        // write contents back to .Manifest
        strcpy(projectName, temp_project_name);
        fd = open(manifestPath(projectName), O_RDWR | O_CREAT | O_TRUNC, 00600);
        LL_to_manifest(head, fd);
        
        //free list
        close(fd);
        if(modify == 1) // did not add
        {
            return 0;
        }
        return 1; // successful add
    }


}

int Remove(char* projectName, char* fileName)
{
    if(!isDirectoryExists(projectName))
    {
        return 0; // project does not exist on client side
    }
    else
    {
        char* temp_file_name = malloc(sizeof(char) * 50);
        strcpy(temp_file_name, fileName);
        char* temp_project_name = malloc(sizeof(char) * 50);
        strcpy(temp_project_name, projectName);
        int fd = open(manifestPath(projectName), O_RDONLY);

        if(fd == -1)
        {
            printf("Manifest does not exist\n");
            return 0;
        }

        node* head = manifest_to_LL(fd);
        int removed = 0;
        // go through LL with fileName as key
        strcpy(fileName, temp_file_name);
        node* ptr = head;
        while(ptr != NULL)
        {
            if(strcmp(ptr->pathName, filePath(temp_project_name, fileName)) == 0)
            {
                strcpy(ptr->status, "R");
                removed = 1;
                break;
            }
            ptr = ptr->next;
        }

        close(fd);

        // write contents back to .Manifest
        strcpy(projectName, temp_project_name);
        fd = open(manifestPath(projectName), O_RDWR | O_CREAT | O_TRUNC, 00600);
        LL_to_manifest(head, fd);
        
        //free list
        close(fd);
        if(removed == 0) // no match found
        { 
            printf("File not found in manifest\n");
            return 0;
        }
        return 1;
    }
    
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

int sendMessage(char* cmd, int sockFD)
{
    int size = strlen(cmd);
    char* msg = (char*) malloc(sizeof(char) * (size + numDigits(size) + 2));
    bzero(msg, size + numDigits(size) + 2);
    sprintf(msg, "%d:", size);
    strcat(msg, cmd);
    printf("%s\n", msg);
    printf("%d\n", strlen(msg));
    write(sockFD, msg, strlen(msg));
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

void makePath(char* filePath)
{
    char* endIndex = strrchr(filePath, '/');
    char* dirPath = (char*) malloc(sizeof(char) * strlen(filePath));
    bzero(dirPath, strlen(filePath));
    while(filePath != endIndex)
    {
        char* c = (char*) malloc(sizeof(char) * 2);
        c[0] = *filePath;
        c[1] = '\0';
        strcat(dirPath, c);
        filePath++;
    }
    char* command = (char*) malloc(sizeof(char) * (strlen(dirPath) + 10));
    bzero(command, strlen(dirPath) + 10);
    sprintf(command, "mkdir -p %s", dirPath);
    system(command);
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

void createSentFiles(char* buffer)
{
    char numFilesStr[5];
    bzero(numFilesStr, 5);
    int bufferIndex = 0;
    int i = 0;

    while(buffer[bufferIndex] != ':')
    {
        numFilesStr[i] = buffer[bufferIndex];
        bufferIndex++;
        i++;
    }

    bufferIndex++;

    int numFiles = atoi(numFilesStr);
    
    int ind = 0;
    for(ind = 0; ind < numFiles; ind++)
    {
        i = 0;
        char* pathSizeStr = (char*) malloc(sizeof(char) * (i+1));
        while(buffer[bufferIndex] != ':')
        {
            pathSizeStr[i] = buffer[bufferIndex];
            i++;
            bufferIndex++;
            char* t = realloc(pathSizeStr, i+1);
            pathSizeStr = t;
            pathSizeStr[i] = '\0';
        }
        bufferIndex++;
        int pathSize = atoi(pathSizeStr);
        free(pathSizeStr);

        char* path = (char*) malloc(sizeof(char) * (pathSize+1));
        bzero(path, pathSize+1);
        for(i = 0; i < pathSize; i++)
        {
            path[i] = buffer[bufferIndex];
            bufferIndex++;
        }
        bufferIndex++;
        makePath(path);
        i = 0;
        char* dataSizeStr = (char*) malloc(sizeof(char) * (i+1));
        while(buffer[bufferIndex] != ':')
        {
            dataSizeStr[i] = buffer[bufferIndex];
            i++;
            bufferIndex++;
            char* t = realloc(dataSizeStr, i+1);
            dataSizeStr = t;
            dataSizeStr[i] = '\0';
        }
        bufferIndex++;
        int dataSize = atoi(dataSizeStr);
        free(dataSizeStr);
        char* data = (char*) malloc(sizeof(char) * (dataSize + 1));
        bzero(data, dataSize+1);
        printf("%d\n", dataSize);
        for(i = 0; i < dataSize; i++)
        {
            data[i] = buffer[bufferIndex];
            bufferIndex++;
        }
        bufferIndex++;
        int fd = open(path, O_RDWR | O_CREAT, 00600);
        write(fd, data, strlen(data));
        close(fd);
        free(data);
        free(path);
    }
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
            int sockFD = create_socket();
            char* createCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(createCommand, strlen(argv[2])+4);
            sprintf(createCommand, "cr:%s", argv[2]);
            sendMessage(createCommand, sockFD);
            
            char* serverResponse = readMessage(serverResponse, sockFD);

            char* tokens = strtok(serverResponse, ":");
            if(strcmp(tokens, "sc") == 0)
            {
                mkdir(argv[2], 00777);
                char* path = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 20));
                sprintf(path, "%s/.Manifest", argv[2]);
                printf("%s\n", path);
                int fd = open(path, O_RDWR | O_CREAT, 00600);
                write(fd, "0\n", 2);
                printf("Create command completed\n");
                close(fd);
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
            sendMessage(deleteCommand, sockFD);

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

        if(strcmp(argv[1], "add") == 0)
        {
            int success = add(argv[2], argv[3]);
            if(success == 1)
            {
                printf("add successful\n");
            }
            else{
                printf("add failed\n");
            }
        }

        if(strcmp(argv[1], "remove") == 0)
        {
            int success = Remove(argv[2], argv[3]);
            if(success == 1)
            {
                printf("remove successful\n");
            }
            else{
                printf("remove failed\n");
            }
        }

        if(strcmp(argv[1], "currentversion") == 0)
        {
            int sockFD = create_socket();
            char* cvCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(cvCommand, strlen(argv[2]) + 4);
            sprintf(cvCommand, "cv:%s", argv[2]);
            sendMessage(cvCommand, sockFD);
            char* serverResponse = readMessage(serverResponse, sockFD);
            char* tokens = strtok(serverResponse, ":");
            if(strcmp(tokens, "sc") == 0)
            {
                printf("Current version:\n");
                printf("%s", &serverResponse[3]);
            }
            else if(strcmp(tokens, "er") == 0)
            {
                tokens = strtok(NULL, ":");
                printf("Error: %s\n", tokens);
            }

            close(sockFD);
        }

        if(strcmp(argv[1], "checkout") == 0)
        {
            if(isDirectoryExists(argv[2]))
            {
                printf("Error: Project already exists on client\n");
                return 0;
            }
            int sockFD = create_socket();
            char* coCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(coCommand, strlen(argv[2]) + 4);
            sprintf(coCommand, "co:%s", argv[2]);
            sendMessage(coCommand, sockFD);
            free(coCommand);

            char* serverResponse = readMessage(serverResponse, sockFD);
            char* tokens = strtok(serverResponse, ":");
            printf("%s\n", serverResponse);
            if(strcmp(tokens, "sf") == 0)
            {
                printf("Checkout successful\n");
                createSentFiles(&serverResponse[3]);
            }
            close(sockFD);
            free(serverResponse);
        }

        if(strcmp(argv[1], "commit") == 0)
        {
            if(!isDirectoryExists(argv[2]))
            {
                printf("Error: project does not exist\n");
                return 0;
            }
            int sockFD = create_socket();
            char* cmCommand = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 4));
            bzero(cmCommand, strlen(argv[2]) + 4);
            sprintf(cmCommand, "cm:%s", argv[2]);
            sendMessage(cmCommand, sockFD);
            char* serverResponse = readMessage(serverResponse, sockFD);
            printf("%s\n", serverResponse);
            //here is where we need to create a LL of the server's manifest
        }
        
    }

    return 0;
}