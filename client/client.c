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
#include <openssl/md5.h>
#include<errno.h>


typedef struct node
{
    char* status;
    char* pathName;
    char* versionNum;
    unsigned char* hash;
    struct node* next;

} node;

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

void non_blocking_write(char* buffer, int size, int fd)
{
    int status = 1;
    int bytesWrote = 0;

    do
    {
        status = write(fd, buffer+bytesWrote, size-bytesWrote);
        bytesWrote += status;
    } while (status > 0 && bytesWrote < size);
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

int isDirectoryExists(const char* path) 
{
    struct stat stats;
    stat(path, &stats);

    if(S_ISDIR(stats.st_mode))
        return 1;
    
    return 0;
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
    string = ptr->status;
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
        char* string = malloc(sizeof(char) * 20);

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
            fileContents = non_blocking_read(fileContents, fd2);
            
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
        
    }

    return 0;
}