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

char* getHash(char* fileContents)
{
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

    free(result);

    return hashedStr;
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
    tmp->pathName = malloc(sizeof(char) * 1000);
    tmp->versionNum = malloc(sizeof(char) * 1000);
    tmp->hash = malloc(sizeof(char) * 1000);
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

char* getManifestPath(char* projectName)
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
    char* result = malloc(sizeof(char) * ( strlen(projectName) + strlen(fileName) + 2));
    bzero(result, strlen(projectName) + strlen(fileName) + 2 );
    memcpy(result, projectName, strlen(projectName));
    strcat(result, "/");
    strcat(result, fileName);
    return result; 
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

void LL_to_manifest(node* head, int fd)
{
    node* ptr = head;
    while(ptr != NULL)
    {
        char* string = malloc(sizeof(char) * 1000);
        bzero(string, 1000);

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



void freeList(node* head)
{
    node* ptr = head;
    while( ptr != NULL)
    {
        node* temp = ptr;
        ptr = ptr->next;
        /*
        free(temp->status);
        free(temp->pathName);
        free(temp->versionNum);
        free(temp->hash);
        */
        free(temp); 
    }
}


int add(char* projectName, char* fileName) // returns 1 on success 0 on failure
{ 
    if(!isDirectoryExists(projectName))
    {
        return 0; // project does not exist on client side
    }
    else{
        
        int fd = open(getManifestPath(projectName), O_RDONLY);

        if(fd == -1)
        {
            printf("Manifest does not exist\n");
            return 0;
        }  

        // read contents of manifest into a buffer
        char* buffer = (char*) malloc(sizeof(char) * 50);
        buffer = readFile(buffer, fd);
        node* head = manifest_to_LL(buffer);
        

        // go through LL with fileName as key
        
        node* ptr = head;
        int modify = 0;
        while(ptr != NULL){
            if(strcmp(ptr->pathName, filePath(projectName, fileName)) == 0) // match
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
            
            int fd2 = open(filePath(projectName, fileName), O_RDONLY);
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
            
            char* hashedStr = (char*)malloc(sizeof(char)*33);
            hashedStr = getHash(fileContents);

            node* resultNode = malloc(sizeof(node));
            resultNode = nullNode(resultNode);
            strcpy(resultNode->status, "A"); // add
            strcpy(resultNode->pathName, filePath(projectName, fileName));
            strcpy(resultNode->versionNum, "0");
            strcpy(resultNode->hash, hashedStr);

            ptr->next = resultNode; // adds node that represents last line in manifest

            close(fd2);
            
        }

        close(fd);
        
        // write contents back to .Manifest
        fd = open(getManifestPath(projectName), O_RDWR | O_CREAT | O_TRUNC, 00600);
        LL_to_manifest(head, fd);
        
        //free list
        freeList(head);
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
        
        int fd = open(getManifestPath(projectName), O_RDONLY);

        if(fd == -1)
        {
            printf("Manifest does not exist\n");
            return 0;
        }

        // go through contents of manifest and put them into a buffer
        char* buffer = (char*)malloc(sizeof(char) * 20);
        buffer = readFile(buffer, fd);
        node* head = manifest_to_LL(buffer);

        int removed = 0;
        // go through LL with fileName as key
        
        node* ptr = head;
        while(ptr != NULL)
        {
            if(strcmp(ptr->pathName, filePath(projectName, fileName)) == 0)
            {
                strcpy(ptr->status, "R");
                removed = 1;
                break;
            }
            ptr = ptr->next;
        }

        close(fd);

        // write contents back to .Manifest
    
        fd = open(getManifestPath(projectName), O_RDWR | O_CREAT | O_TRUNC, 00600);
        LL_to_manifest(head, fd);
        
        //free list
        freeList(head);
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
    printf("Message read is: %s\n", msg);
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
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 00600);
        write(fd, data, strlen(data));
        close(fd);
        free(data);
        free(path);
    }
}


//0: added entry (keep hash), 1: modified entry (new hash), 2: delete entry, 4: no change, 4: entry not in server but deleted from client (no change)
//3: client modified but not in server, so it is techinically an Add but with a live hash (new hash)
int compareClientAndServerManifests(node* serverManifest, node* clientEntry)
{
    node* ptr = serverManifest;

    while(ptr != NULL)
    {
        if(strcmp(ptr->pathName, clientEntry->pathName) == 0)
        {
            if(strcmp(clientEntry->status, "R") == 0)
            {
                printf("D ");
                printf(clientEntry->pathName);
                printf("\n");
                return 2;
            }
            
            if(strcmp(ptr->hash, clientEntry->hash) == 0)
            {
                int fd = open(clientEntry->pathName, O_RDONLY);
                char* fileContents = readFile(fileContents, fd);
                char* liveHash = getHash(fileContents);
                free(fileContents);
                close(fd);

                if(strcmp(clientEntry->hash, liveHash) != 0)
                {
                    printf("M ");
                    printf(clientEntry->pathName);
                    printf("\n");
                    free(liveHash);
                    return 1;
                }
                else
                    return 4;
            }


        }

        ptr = ptr->next;
    }

    if(strcmp(clientEntry->status, "R") == 0)
        return 4;
    
    printf("A ");
    printf(clientEntry->pathName);
    printf("\n");

    if(strcmp(clientEntry->status, "M") == 0)
        return 3;

    return 0;
}

//-1: C, 1: M, 2: D
int updateCompareClientAndServer(node* serverManifest, node* clientEntry, int updateFD, int conflictFD)
{
    node* ptr = serverManifest;

    while(ptr != NULL)
    {
        if(strcmp(ptr->pathName, clientEntry->pathName) == 0)
        {
            if(strcmp(ptr->versionNum, clientEntry->versionNum) != 0 && strcmp(ptr->hash, clientEntry->hash) != 0)
            {
                int fd = open(clientEntry->pathName, O_RDONLY);
                char* fileContents = readFile(fileContents, fd);
                char* liveHash = getHash(fileContents);
                free(fileContents);
                close(fd);

                if(strcmp(clientEntry->hash, liveHash) == 0)
                {
                    printf("M ");
                    printf(clientEntry->pathName);
                    printf("\n");
                    write(updateFD, "M\t", 2);
                    write(updateFD, ptr->pathName, strlen(ptr->pathName));
                    write(updateFD, "\t", 1);
                    write(updateFD, ptr->versionNum, strlen(ptr->versionNum));
                    write(updateFD, "\t", 1);
                    write(updateFD, ptr->hash, strlen(ptr->hash));
                    write(updateFD, "\n", 1);
                    free(liveHash);
                    return 1;
                }
            }
            else if(strcmp(ptr->hash, clientEntry->hash) != 0)
            {
                int fd = open(clientEntry->pathName, O_RDONLY);
                char* fileContents = readFile(fileContents, fd);
                char* liveHash = getHash(fileContents);
                free(fileContents);
                close(fd);

                if(strcmp(clientEntry->hash, liveHash) != 0)
                {
                    printf("C ");
                    printf(clientEntry->pathName);
                    printf("\n");
                    write(conflictFD, "C\t", 2);
                    write(conflictFD, clientEntry->pathName, strlen(clientEntry->pathName));
                    write(conflictFD, "\t", 1);
                    write(conflictFD, clientEntry->versionNum, strlen(clientEntry->versionNum));
                    write(conflictFD, "\t", 1);
                    write(conflictFD, liveHash, strlen(liveHash));
                    write(conflictFD, "\n", 1);
                    free(liveHash);
                    return -1;
                }
            }
        }

        ptr = ptr->next;
    }

    printf("D ");
    printf(clientEntry->pathName);
    printf("\n");

    write(updateFD, "D\t", 2);
    write(updateFD, clientEntry->pathName, strlen(clientEntry->pathName));
    write(updateFD, "\t", 1);
    write(updateFD, clientEntry->versionNum, strlen(clientEntry->versionNum));
    write(updateFD, "\t", 1);
    write(updateFD, clientEntry->hash, strlen(clientEntry->hash));
    write(updateFD, "\n", 1);

    return 2;
}

//1: add
int updateCompareServerAndClient(node* clientManifest, node* serverEntry, int fd)
{
    node* ptr = clientManifest;

    while(ptr != NULL)
    {
        if(strcmp(ptr->pathName, serverEntry->pathName) == 0)
            return 0;
        
        ptr = ptr->next;
    }

    printf("A ");
    printf(serverEntry->pathName);
    printf("\n");

    write(fd, "A\t", 2);
    write(fd, serverEntry->pathName, strlen(serverEntry->pathName));
    write(fd, "\t", 1);
    write(fd, serverEntry->versionNum, strlen(serverEntry->versionNum));
    write(fd, "\t", 1);
    write(fd, serverEntry->hash, strlen(serverEntry->hash));
    write(fd, "\n", 1);

    return 1;
}

char* int_to_string(int x)
{
    char* str = (char*)malloc(sizeof(char) * (numDigits(x) + 1));
    bzero(str, numDigits(x) + 1);
    sprintf(str, "%d", x);
    return str;
}

void free_fileLL(FileNode* fileHead)
{
    FileNode* fptr = fileHead;
    while(fptr != NULL)
    {
        FileNode* temp_fptr = fptr;
        fptr = fptr->next;
        /*
        free(fptr->contents);
        free(fptr->pathName);
        */
        free(temp_fptr);
    }
}

void removeCommit(char* commitPath)
{
    char* remove_commit = (char*)malloc(sizeof(char) * (strlen(commitPath) + 7));
    bzero(remove_commit, strlen(commitPath) + 7);
    sprintf(remove_commit, "rm -r %s", commitPath);
    system(remove_commit);
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
                int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 00600);
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
            serverResponse = &serverResponse[3];

            char* idStr = (char*) malloc(sizeof(char));
            idStr[0] = '\0'; 
            int index = 0;

            while(serverResponse[index] != ':')
            {
                idStr[index] = serverResponse[index];
                char* tmp = (char*) malloc(sizeof(char) * (index+1+1));
                memset(tmp, '\0', index+1+1);
                memcpy(tmp, idStr, index+1);
                free(idStr);
                idStr = tmp;
                index++;
            }

            node* serverManifest = manifest_to_LL(serverResponse);
            char* clientManifestPath = (char*) malloc(sizeof(char) * strlen(argv[2]) + 20);
            bzero(clientManifestPath, strlen(argv[2]) + 20);
            sprintf(clientManifestPath, "%s/.Manifest", argv[2]);
            int clientManifestFD = open(clientManifestPath, O_RDONLY);
            char* clientManifestContents = readFile(clientManifestContents, clientManifestFD);
            node* clientManifest = manifest_to_LL(clientManifestContents);
            close(clientManifestFD);
            free(clientManifestContents);
            //here is where we compare the 2 manifests

            char* clientCommitPath = (char*) malloc(sizeof(char) * strlen(argv[2]) + 20);
            bzero(clientCommitPath, strlen(argv[2]) + 20);
            sprintf(clientCommitPath, "%s/.Commit", argv[2]);

            int clientCommitFD = open(clientCommitPath, O_RDWR | O_CREAT | O_TRUNC, 00600);

            int clientVersion = atoi(clientManifest->versionNum);
            int serverVersion = atoi(serverManifest->versionNum);

            write(clientCommitFD, idStr, strlen(idStr));
            write(clientCommitFD, "\n", 1);

            node* ptr = clientManifest->next;

            while(ptr != NULL)
            {
                int response = compareClientAndServerManifests(serverManifest->next, ptr);
                if(response == 0)
                {
                    write(clientCommitFD, "A", 1);
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->pathName, strlen(ptr->pathName));
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->versionNum, strlen(ptr->versionNum));
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->hash, strlen(ptr->hash));
                    write(clientCommitFD, "\n", 1);
                }
                if(response == 1)
                {
                    write(clientCommitFD, "M", 1);
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->pathName, strlen(ptr->pathName));
                    write(clientCommitFD, "\t", 1);
                    int versionNum = atoi(ptr->versionNum) + 1;
                    char* newVersionNum = (char*) malloc(sizeof(char) * (numDigits(versionNum) + 1));
                    bzero(newVersionNum, numDigits(versionNum)+1);
                    sprintf(newVersionNum, "%d", versionNum);
                    write(clientCommitFD, newVersionNum, strlen(newVersionNum));
                    write(clientCommitFD, "\t", 1);
                    int fd = open(ptr->pathName, O_RDONLY);
                    char* fileContents = readFile(fileContents, fd);
                    close(fd);
                    char* hash = getHash(fileContents);
                    write(clientCommitFD, hash, strlen(hash));
                    write(clientCommitFD, "\n", 1);
                }
                if(response == 2)
                {
                    write(clientCommitFD, "D", 1);
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->pathName, strlen(ptr->pathName));
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->versionNum, strlen(ptr->versionNum));
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->hash, strlen(ptr->hash));
                    write(clientCommitFD, "\n", 1);                    
                }
                if(response == 3)
                {
                    write(clientCommitFD, "A", 1);
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->pathName, strlen(ptr->pathName));
                    write(clientCommitFD, "\t", 1);
                    write(clientCommitFD, ptr->versionNum, strlen(ptr->versionNum));
                    write(clientCommitFD, "\t", 1);
                    int fd = open(ptr->pathName, O_RDONLY);
                    char* fileContents = readFile(fileContents, fd);
                    close(fd);
                    char* hash = getHash(fileContents);
                    write(clientCommitFD, hash, strlen(hash));
                    write(clientCommitFD, "\n", 1);                    
                }
                ptr = ptr->next;
            }
            close(clientCommitFD);
            clientCommitFD = open(clientCommitPath, O_RDONLY);
            char* clientCommitContents = readFile(clientCommitContents, clientCommitFD);
            close(clientCommitFD);
            sendMessage(clientCommitContents, sockFD);
        }
        if(strcmp(argv[1], "push") == 0 )
        {
            char* projectName = argv[2];
            char* commitPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 9));
            bzero(commitPath, strlen(projectName) + 9);
            strcpy(commitPath, projectName);
            strcat(commitPath, "/.Commit");
            int commitFD = open(commitPath, O_RDONLY);
            int total_message_length;

            if(commitFD == -1)
            {
                printf("No commit file on client side; Client must call commit before pushing\n");
                return 0;
            }

            // make .Commit into a LL
            char* commitContents = readFile(commitContents, commitFD);

            // check if server has the same .Commit

            // make message to send
            total_message_length += 7; // number of colons + "pu"
            total_message_length += strlen(projectName) + numDigits(strlen(projectName)) + strlen(commitContents) + numDigits(strlen(commitContents));
            char* message = (char*)malloc( sizeof(char) * (total_message_length + 1) ); // +1 is for \0
            bzero(message, total_message_length + 1);
            sprintf(message, "%s", "pu:");
            char* project_name_length = (char*)malloc(sizeof(char) * ( numDigits(strlen(projectName)) + 1) );
            bzero(project_name_length, numDigits(strlen(projectName)) + 1);
            sprintf(project_name_length, "%d", strlen(projectName));
            strcat(message, project_name_length);
            strcat(message, ":");
            strcat(message, projectName);
            strcat(message, ":");
            char* commit_length = (char*)malloc(sizeof(char) * (numDigits(strlen(commitContents)) + 1) );
            bzero(commit_length, numDigits(strlen(commitContents)) + 1);
            sprintf(commit_length, "%d", strlen(commitContents));
            strcat(message, commit_length);
            strcat(message, ":");
            strcat(message, commitContents);

            printf("Sending server:\n%s", message);
            
            
            // open socket
            int serverFD = create_socket();
            sendMessage(message, serverFD);
            
            
            char* serverResponse = readMessage(serverResponse, serverFD);
            printf("Server response is: %s\n", serverResponse);
            
            if(strcmp(serverResponse, "er:project does not exist on the server") == 0) // project was not on server
            {
                printf("Error: Project did not exist on server\n");
                removeCommit(commitPath);
                return 0;    
            }
            if(strcmp(serverResponse, "er:no match for .Commit was found") == 0) // project was not on server
            {
                printf("Error: No match for client's .Commit was found on the server\n");
                removeCommit(commitPath);
                return 0;    
            }
            if(strcmp(serverResponse, "su:match for .Commit was found") == 0)
            {
                printf("Success: Match for client's .Commit found on server\n");
            }

            
            node* commitHead = NULL;
            commitHead = manifest_to_LL(commitContents); // change name from manifest_to_LL to file_to_LL later
            
            free(message);
            free(serverResponse);
            free(commitContents);
            close(commitFD);

            //make FileNode LL from commit LL
            node* ptr = commitHead;
            FileNode* fileHead = NULL;
            int fd;
            while(ptr != NULL)
            {
                if(ptr == commitHead) // commit clientid(versionNum)
                {
                    ptr = ptr->next;
                    continue;
                }

                else
                {
                    //printf("file to open is: %s\n", ptr->pathName);
                    fd = open(ptr->pathName, O_RDONLY);
                    if(fd == -1)
                    {
                        printf("File in .Commit does not exist\n");
                        removeCommit(commitPath);
                        return 0;
                    }
                    char* fileContents = readFile(fileContents, fd);
                    printf("Contents of %s is %s\n", ptr->pathName, fileContents);
                    close(fd);

                    FileNode* tempfnode = malloc(sizeof(FileNode));
                    tempfnode->contents = (char*)malloc(sizeof(char) * (strlen(fileContents) + 1));
                    bzero(tempfnode->contents, strlen(fileContents) + 1);
                    strcpy(tempfnode->contents, fileContents);
                    tempfnode->pathName = (char*)malloc(sizeof(char) * (strlen(ptr->pathName)+1));
                    bzero(tempfnode->pathName, strlen(ptr->pathName)+1);
                    strcpy(tempfnode->pathName, ptr->pathName);
                    tempfnode->size = strlen(fileContents);
                    tempfnode->next = NULL;

                    if(fileHead == NULL)
                    {
                        fileHead = tempfnode;    // make head of file LL
                    } 
                    else
                    {
                        FileNode* fptr = fileHead;
                        while(fptr->next != NULL)
                        {
                            fptr = fptr->next;
                        }  
                        fptr->next = tempfnode; // add to end of file LL  
                    }
                           
                }

                ptr = ptr->next;
            }
            
            
            // Make file content message to send to the server

            
            int numFiles = 0;
            FileNode* fptr = fileHead;
            char* fileMessage = (char*)malloc(sizeof(char) * 1000); // fix later to malloc #bytes based on file size
            bzero(fileMessage, 1000);

            fptr = fileHead;
            while(fptr != NULL)
            {
                
                char* file_name_length = int_to_string(strlen(fptr->pathName));
                char* file_contents_length = int_to_string(fptr->size);
                strcat(fileMessage, file_name_length);
                strcat(fileMessage, ":");
                strcat(fileMessage, fptr->pathName);
                strcat(fileMessage, ":");
                strcat(fileMessage, file_contents_length);
                strcat(fileMessage, ":");
                strcat(fileMessage, fptr->contents);     
                
                if(fptr->next != NULL) // if not the last file in the list, then add a colon
                {
                    strcat(fileMessage, ":");
                }
                fptr = fptr->next; 
                numFiles++;      
            }

            char* num_of_files = int_to_string(numFiles);
            char* secondMessage = (char*)malloc(sizeof(char) * (strlen(num_of_files) + strlen(fileMessage) + 2));
            bzero(secondMessage, strlen(num_of_files) + strlen(fileMessage) + 2);
            sprintf(secondMessage, "%s:%s", num_of_files, fileMessage);
            printf("Message to be sent to server is: %s\n", secondMessage);

            free_fileLL(fileHead);
            sendMessage(secondMessage, serverFD);
            freeList(commitHead);
            free(fileMessage);
            
            char* finalResponse = readMessage(finalResponse, serverFD);
            printf("The finalResponse is: %s\n", finalResponse);
            
            int i = 0;
            char* push_result = (char*)malloc(sizeof(char) * 3);
            bzero(push_result, 3);

            while(finalResponse[i] != ':')
            {
                push_result[i] = finalResponse[i];
                i++;
            }
            push_result[i] = '\0';
            printf("push_result is: %s\n", push_result);
            i++; // skip : after er: or su:
            if(strcmp(push_result, "er") == 0)
            {
                printf("Push failed\n");
                removeCommit(commitPath);
            }
            else if(strcmp(push_result, "su") == 0) // get new manifest contents from message and overwrite manifest
            {
                
                int strIndex = 0;
                char* manifest_length = (char*)malloc(sizeof(char) * 100);
                bzero(manifest_length, 100);
                while(finalResponse[i] != ':')
                {
                    manifest_length[strIndex] = finalResponse[i];
                    strIndex++;
                    i++;
                }
                manifest_length[strIndex] = '\0';
                int man_len = atoi(manifest_length);
                strIndex = 0;
                i++; // skip : after manifest length to get to start of contents
                char* manifest_contents = (char*)malloc(sizeof(char) * (man_len + 1));
                bzero(manifest_contents, man_len + 1);
                while(strIndex < man_len)
                {
                    manifest_contents[strIndex] = finalResponse[i];
                    strIndex++;
                    i++;
                }
                manifest_contents[strIndex] = '\0';

                int manifestFD = open(getManifestPath(projectName), O_RDWR | O_CREAT | O_TRUNC, 00600 );
                if(manifestFD == -1)
                {
                    printf("No .Manifest found in client's project\n");
                    return 0;
                }
                write(manifestFD, manifest_contents, strlen(manifest_contents));

                // delete copy of .Commit

                removeCommit(commitPath);
                close(serverFD);
                printf("Successful push\n");
            }
            
        }
        if(strcmp(argv[1] , "history") == 0)
        {
            char* projectName = argv[2];
            char* project_name_length = int_to_string(strlen(projectName));

            int serverFD = create_socket();
            
            char* message = (char*)malloc(sizeof(char) * (strlen(projectName) + strlen(project_name_length) + 5));
            bzero(message, strlen(projectName) + strlen(project_name_length) + 5);
            strcpy(message, "hs:");
            strcat(message, project_name_length);
            strcat(message, ":");
            strcat(message, projectName);

            sendMessage(message, serverFD);

            char* response = readMessage(response, serverFD);

            int index = 0;
            char* status = (char*)malloc(sizeof(char) * 3);
            bzero(status, 3);
            while(response[index] != ':')
            {
                status[index] = response[index];
                index++;
            }
            status[index] = '\0';
            index++; // skip : after status (either er or su)

            if(strcmp(status, "er") == 0)
            {
                printf("Failed to get history of %s from server\n", projectName);
                close(serverFD);
                return 0;
            }
            if(strcmp(status, "su") == 0)
            {
                char* history_length = (char*)malloc(sizeof(char) * strlen(response));
                bzero(history_length, strlen(response));
                int strIndex = 0;
                while(response[index] != ':')
                {
                    history_length[strIndex] = response[index];
                    strIndex++;
                    index++;
                }
                history_length[strIndex] = '\0';
                int history_len = atoi(history_length);
                index++; // skip : after history_length

                strIndex = 0;
                char* historyContents = (char*)malloc(sizeof(char) * (history_len + 1));
                bzero(historyContents, history_len + 1);
                while(strIndex < history_len)
                {
                    historyContents[strIndex] = response[index];
                    strIndex++;
                    index++;
                }
                historyContents[strIndex] = '\0';
                printf("History of %s:\n%s", projectName, historyContents);
            }
        }
        if(strcmp(argv[1], "rollback") == 0)
        {
            char* projectName = argv[2];
            char* projectVersion = argv[3];
            char* project_name_length = int_to_string(strlen(projectName));
            char* version_length = int_to_string(strlen(projectVersion));

            int serverFD = create_socket();

            // malloc space for message to request rollback of a project with specified version number

            char* message = (char*)malloc(sizeof(char) * (strlen(projectName) + strlen(projectVersion) + strlen(project_name_length) + strlen(version_length) + 7));
            bzero(message, strlen(projectName) + strlen(projectVersion) + strlen(project_name_length) + strlen(version_length) + 7 );
            strcpy(message, "ro:");
            strcat(message, project_name_length);
            strcat(message, ":");
            strcat(message, projectName);
            strcat(message, ":");
            strcat(message, version_length);
            strcat(message, ":");
            strcat(message, projectVersion);

            printf("Sending server request to rollback\n");
            printf("Message to be sent to server is %s\n", message);

            sendMessage(message, serverFD);
            char* responseStatus = readMessage(responseStatus, serverFD);
            printf("Server sent: %s\n", responseStatus);
            int index = 0;
            char* status = (char*)malloc(sizeof(char) * 3);
            bzero(status, 3);
            while(responseStatus[index] != ':')
            {
                status[index] = responseStatus[index];
                index++;
            }
            status[index] = '\0';

            if(strcmp(status, "er") == 0)
            {
                printf("Rollback failed\n");
            }
            if(strcmp(status, "su") == 0)
            {
                printf("Rollback successful\n");
            }
            close(serverFD);

        }

        if(strcmp(argv[1], "update") == 0)
        {
            int serverFD = create_socket();

            if(!isDirectoryExists(argv[2]))
            {
                printf("Project does not exist\n");
                close(serverFD);
                return 0;
            }

            char* message = (char*) malloc(sizeof(char) * strlen(argv[2]) + 5);
            bzero(message, strlen(argv[2]) + 5);
            sprintf(message, "ud:%s", argv[2]);
            sendMessage(message, serverFD);
            free(message);

            char* serverResponse = readMessage(serverResponse, serverFD);
            
            if(serverResponse[0] == 'e' && serverResponse[1] == 'r' && serverResponse[2] == ':')
            {
                printf("Error: Project does not exist on server\n");
                free(serverResponse);
                return 0;
            }

            char* manifestPath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
            bzero(manifestPath, strlen(argv[2]) + 12);
            sprintf(manifestPath, "%s/.Manifest", argv[2]);
            int manifestFD = open(manifestPath, O_RDONLY);
            char* clientManifestContents = readFile(clientManifestContents, manifestFD);
            close(manifestFD);
            free(manifestPath);

            node* serverManifest = manifest_to_LL(serverResponse);
            node* clientManifest = manifest_to_LL(clientManifestContents);

            free(clientManifestContents);
            free(serverResponse);

            if(strcmp(serverManifest->versionNum, clientManifest->versionNum) == 0)
            {
                printf("Up To Date\n");
                
                char* updatePath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
                bzero(updatePath, strlen(argv[2]) + 12);
                sprintf(updatePath, "%s/.Update", argv[2]);
                int updateFD = open(updatePath, O_RDWR | O_CREAT | O_TRUNC, 00600);
                write(updateFD, "0\n", 2);
                close(updateFD);
                free(updatePath);

                char* conflictPath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
                bzero(conflictPath, strlen(argv[2]) + 12);
                sprintf(conflictPath, "%s/.Conflict", argv[2]);
                remove(conflictPath);
                free(conflictPath);

                freeList(serverManifest);
                freeList(clientManifest);
                return 0;
            }
            
            char* updatePath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
            bzero(updatePath, strlen(argv[2]) + 12);
            sprintf(updatePath, "%s/.Update", argv[2]);
            int updateFD = open(updatePath, O_RDWR | O_CREAT | O_TRUNC, 00600);
            write(updateFD, "0\n", 2);
            free(updatePath);
            
            char* conflictPath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
            bzero(conflictPath, strlen(argv[2]) + 12);
            sprintf(conflictPath, "%s/.Conflict", argv[2]);
            int conflictFD = open(conflictPath, O_RDWR | O_CREAT | O_TRUNC, 00600);

            node* clientPtr = clientManifest->next;
            node* serverPtr = serverManifest->next;

            while(clientPtr != NULL)
            {
                serverPtr = serverManifest->next;
                int code = updateCompareClientAndServer(serverPtr, clientPtr, updateFD, conflictFD);
                clientPtr = clientPtr->next;
            }

            serverPtr = serverManifest->next;

            while(serverPtr != NULL)
            {
                clientPtr = clientManifest->next;
                int code = updateCompareServerAndClient(clientPtr, serverPtr, updateFD);
                serverPtr = serverPtr->next;
            }

            close(conflictFD);
            close(updateFD);

            int conflictSize = getFileSize(conflictPath);
            if(conflictSize > 0)
                printf("Conflicts were found, must be resolved before project is updated\n");
            else if(conflictSize == 0)
                remove(conflictPath);
            free(conflictPath);
            freeList(clientManifest);
            freeList(serverManifest);

            close(serverFD);
        }

        if(strcmp(argv[1], "upgrade") == 0)
        {
            if(!isDirectoryExists(argv[2]))
            {
                printf("Error: Project does not exist\n");
                return 0;
            }

            char* conflictPath = (char*) malloc(sizeof(char) * (strlen(argv[2]) + 12));
            bzero(conflictPath, strlen(argv[2]) + 12);
            sprintf(conflictPath, "%s/.Conflict", argv[2]);

            int conflictFD = open(conflictPath, O_RDONLY);
            if(conflictFD != -1)
            {
                close(conflictFD);
                printf("Error: There are conflicts. Resolve them first and update again\n");
                free(conflictPath);
                return 0;
            }
            free(conflictPath);

            char* updatePath = (char*) malloc(sizeof(char) * strlen(argv[2]) + 12);
            bzero(updatePath, strlen(argv[2]) + 12);
            sprintf(updatePath, "%s/.Update", argv[2]);

            int updateFD = open(updatePath, O_RDONLY);
            if(updateFD == -1)
            {
                printf("Error: Update was not called, please call it before upgrading\n");
                free(updatePath);
                return 0;
            }

            char* updateContents = readFile(updateContents, updateFD);
            close(updateFD);

            node* updateList = manifest_to_LL(updateContents);

            if(updateList->next == NULL)
            {
                printf("Project is up to date\n");
                remove(updatePath);
                free(updatePath);
                free(updateContents);
                freeList(updateList);
                return 0;
            }

            char* projectName = argv[2];
            char* project_name_length = int_to_string(strlen(projectName));

            int serverFD = create_socket();
            
            char* message = (char*)malloc(sizeof(char) * (strlen(projectName) + strlen(project_name_length) + 5));
            bzero(message, strlen(projectName) + strlen(project_name_length) + 5);
            strcpy(message, "ug:");
            strcat(message, project_name_length);
            strcat(message, ":");
            strcat(message, projectName);

            sendMessage(message, serverFD);
            free(message);
            char* projectExists = readMessage(projectExists, serverFD);

            if(projectExists[0] == 'e' && projectExists[0] == 'r' && projectExists[0] == ':')
            {
                printf("Server sent: %s\n", projectExists);
                free(projectExists);
                return 0;
            }
            if(projectExists[0] == 's' && projectExists[0] == 'u' && projectExists[0] == ':')
            {
                printf("Server sent: %s\n", projectExists);
            }

            int numFiles = 0;
            node* update_ptr = updateList->next;
            FileNode* fileList = NULL;
            while(update_ptr != NULL) // go through update list and 
            {
                if(strcmp(update_ptr->status, "D") == 0)
                {
                    remove(update_ptr->pathName); // removes file from client's local project copy / update manifest later baseed on .Update
                }
                if(strcmp(update_ptr->status, "A") == 0 || strcmp(update_ptr->status, "M") == 0)
                {
                    FileNode* tempfnode = (FileNode*)malloc(sizeof(FileNode));
                    tempfnode->pathName = (char*)malloc(sizeof(char) * (strlen(update_ptr->pathName) + 1));
                    bzero(tempfnode->pathName, strlen(update_ptr->pathName) + 1);
                    strcpy(tempfnode->pathName, update_ptr->pathName);
                    tempfnode->contents = NULL; // don't need to send the file contents to the server
                    tempfnode->next = NULL;

                    if(fileList == NULL) // need to assign head
                    {
                        fileList = tempfnode;
                    }
                    else
                    {
                        FileNode* traversal = fileList;
                        while(traversal->next != NULL)
                        {
                            traversal = traversal->next;
                        }
                        traversal->next = tempfnode;
                    }
                    
                }
                update_ptr = update_ptr->next;
            }

            // build message to request files from server from fileList
            // numFiles:fileName_length:fileName:fileName2_length:fileName2

            int total_message_length = 0;
            FileNode* file_traversal = fileList;
            while(file_traversal != NULL)
            {
                numFiles++;
                total_message_length += numDigits(strlen(file_traversal->pathName)) + 1 + strlen(file_traversal->pathName);

                if(file_traversal->next != NULL) // not the last file name in the message
                {
                    total_message_length++; // for the colon
                }
                file_traversal = file_traversal->next;
            }
            total_message_length += numDigits(numFiles) + 1;

            char* fileRequest = (char*)malloc(sizeof(char) * (total_message_length + 1));
            bzero(fileRequest, total_message_length + 1);

            char* num_files = int_to_string(numFiles);
            strcpy(fileRequest, num_files);
            strcat(fileRequest, ":");

            file_traversal = fileList;
            while(file_traversal != NULL)
            {
                char* file_name_length = int_to_string(strlen(file_traversal->pathName));
                strcat(fileRequest, file_name_length);
                strcat(fileRequest, ":");
                strcat(fileRequest, file_traversal->pathName);
                if(file_traversal->next != NULL) // not the last file in the message so add a colon
                {
                    strcat(fileRequest, ":");
                }
                file_traversal = file_traversal->next;
            }

            free_fileLL(fileList);
            printf("Sending server request: %s\n", fileRequest);
            sendMessage(fileRequest, serverFD );
            free(fileRequest);



        }
        
    }

    return 0;
}