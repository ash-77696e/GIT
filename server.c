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
#include <pthread.h>
#include "structs.h"
#include <signal.h>
#include "IO.h"
#include <openssl/md5.h>

int clientIds;
CommitNode* commitNodes;

typedef struct thread_node
{
    pthread_t thread_id;
    struct thread_node* next;
} thread_node;

thread_node* threadFront = NULL;
pthread_mutex_t thread_lock;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

node* nullNode(node* tmp)
{
    tmp->status = malloc(sizeof(char) * 20);
    bzero(tmp->status, 20);
    tmp->pathName = malloc(sizeof(char) * 1000);
    bzero(tmp->pathName, 1000);
    tmp->versionNum = malloc(sizeof(char) * 1000);
    bzero(tmp->versionNum, 1000);
    tmp->hash = malloc(sizeof(char) * 1000);
    bzero(tmp->hash, 1000);
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

    return hashedStr;
}

char* int_to_string(int x)
{
    char* str = (char*)malloc(sizeof(char) * (numDigits(x) + 1));
    bzero(str, numDigits(x) + 1);
    sprintf(str, "%d", x);
    return str;
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
        free(temp); 
    }
}

void free_fileLL(FileNode* fileHead)
{
    FileNode* fptr = fileHead;
    while(fptr != NULL)
    {
        FileNode* temp_fptr = fptr;
        fptr = fptr->next;
        free(temp_fptr);
    }
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
    //msg[size] = '\0';
    //printf("message size is %d\n", size); // delete later
    //printf("message is %s\n", msg); // delete later
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
    //printf("makePath command is: %s\n", command);
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

char* search_in_fileLL(FileNode* fileHead, char* pathName)
{
    FileNode* ptr = fileHead;
    while(ptr != NULL)
    {
        if(strcmp(ptr->pathName, pathName) == 0)
        {
            return ptr->contents;
        }
        ptr = ptr->next;
    }
    return NULL; // if no match is found
}

CommitNode* expirePendingCommits(CommitNode* head, char* projectName) // returns new head of pending commit list
{
    int deleteHead = 0; // if head should be deleted then delete the head after everything else
    if(head == NULL)
    {
        return NULL;
    }
    if(strcmp(head->projName, projectName) == 0)
    {
        deleteHead = 1;
    }
    CommitNode* ptr = head;
    while(ptr->next != NULL)
    {
        if(strcmp(ptr->next->projName, projectName) == 0) // delete node
        {
            CommitNode* temp = ptr->next;
            ptr->next = ptr->next->next;
            free(temp);
        }
        else
        {
            ptr = ptr->next;
        }   
    }

    if(deleteHead == 1)
    {
        CommitNode* temp = head;
        head = head->next;
        free(temp);
    }
    return head;
}

void untar_project(char* projectName, char* versionNum)
{
    // move tar to current working directory
    char* mv_cmd = (char*)malloc(sizeof(char) * (strlen(projectName) + strlen(versionNum) + 20));
    bzero(mv_cmd, strlen(projectName) + strlen(versionNum) + 20);
    char* tar_path = (char*)malloc(sizeof(char) * (strlen(projectName) + strlen(versionNum) + 14));
    bzero(tar_path, strlen(projectName) + strlen(versionNum) + 14);
    strcpy(tar_path, projectName);
    strcat(tar_path, "/backups/");
    strcat(tar_path, versionNum);
    strcat(tar_path, ".tar");
    sprintf(mv_cmd, "mv %s ./", tar_path);
    //printf("mv_cmd is : %s\n", mv_cmd);
    system(mv_cmd);

    // remove project directory
    char* rm_cmd = (char*)malloc(sizeof(char) * (strlen(projectName) + 7));
    bzero(rm_cmd, strlen(projectName) + 7);
    sprintf(rm_cmd, "rm -r %s", projectName);
    //printf("rm_cmd is : %s\n", rm_cmd);
    system(rm_cmd);

    //untar into current working directory
    char* untar_cmd = (char*)malloc(sizeof(char) * (strlen(versionNum) + 14));
    bzero(untar_cmd, strlen(versionNum) + 14);
    sprintf(untar_cmd, "tar -xvf %s.tar", versionNum);
    //printf("untar_cmd is: %s\n", untar_cmd);
    system(untar_cmd);

    // remove tar in current working directory
    char* rm_tar = (char*)malloc(sizeof(char) * (strlen(versionNum) + 11) );
    bzero(rm_tar, strlen(versionNum) + 11);
    sprintf(rm_tar, "rm -r %s.tar", versionNum);
    //printf("rm_tar is: %s\n", rm_tar);
    system(rm_tar);

}

node* search_in_manLL(node* manifestHead, char* pathName)
{
    node* ptr = manifestHead;
    while(ptr != NULL)
    {
        if(strcmp(ptr->pathName, pathName) == 0)
        {
            return ptr;
        }
        ptr = ptr->next;
    }
    return ptr;
}

node* remove_from_manLL(node* manifestHead, char* pathName)
{
    node* ptr = manifestHead;

    while(ptr->next != NULL)
    {
        if(strcmp(ptr->next->pathName, pathName) == 0)
        {
            ptr->next = ptr->next->next;
            break;
        }
        ptr = ptr->next;
    }

    return manifestHead;

}

node* update_manifest_node(node* manifestHead, node* cmt_ptr, char* to_write)
{
    node* manifest_node = search_in_manLL(manifestHead, cmt_ptr->pathName);
    sprintf(manifest_node->status, "%s", "-"); // reset status
    int file_version = atoi(manifest_node->versionNum);
    file_version++;
    char* file_version_string = int_to_string(file_version);
    strcpy(manifest_node->versionNum, file_version_string); // increment versionNum
    char* file_hash = getHash(to_write);
    strcpy(manifest_node->hash, file_hash); // update hash to what client sent
    return manifestHead;
}

void addToHistory(node* commitHead, char* old_manifest_version, char* projectName)
{
    char* historyPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 10 ));
    bzero(historyPath, strlen(projectName) + 10);
    strcpy(historyPath, projectName);
    strcat(historyPath, "/.History");

    int historyFD = open(historyPath, O_RDWR | O_CREAT | O_APPEND, 00600);

    write(historyFD, old_manifest_version, strlen(old_manifest_version));
    write(historyFD, "\n", 1);

    node* ptr = commitHead->next;
    while(ptr != NULL)
    {
        write(historyFD, ptr->status, strlen(ptr->status));
        write(historyFD, "\t", 1);
        write(historyFD, ptr->pathName, strlen(ptr->pathName));
        write(historyFD, "\t", 1);
        write(historyFD, ptr->hash, strlen(ptr->hash));
        write(historyFD, "\n", 1);
        ptr = ptr->next;
    }
    //write(historyFD, "\n", 1);
    close(historyFD);
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

int canRollback(char* projectName, char* projectVersion)
{
    int manifestFD = open(getManifestPath(projectName), O_RDONLY);

    if(manifestFD == -1)
    {
        return 0;
    }

    char* manifestStream = readFile(manifestStream, manifestFD);
    node* manifestHead = manifest_to_LL(manifestStream);
    int manifest_version = atoi(manifestHead->versionNum);
    int rollback_version = atoi(projectVersion);

    free(manifestStream);
    freeList(manifestHead);
    close(manifestFD);
    if(rollback_version >= manifest_version)
    {
        return 0;
    }
    else
    {
        return 1;
    }
    
}

void deleteBackups(char* projectName)
{
    char* cmd = (char*)malloc(sizeof(char) * (strlen(projectName) + 15));
    bzero(cmd, strlen(projectName) + 15);
    sprintf(cmd, "rm -r %s/backups", projectName);
    system(cmd);
}

int create(char* token, int clientfd)
{
    token = &token[3];
    if(isDirectoryExists(token))
    {
        sendMessage("er:Project exists on server", clientfd);
    }
    mkdir(token, 00777);
    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(token) + 12));
    bzero(manifestPath, strlen(token)+12);
    sprintf(manifestPath, "%s/.Manifest", token);
    //printf("%s\n", manifestPath);
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
        sendMessage("er:Project does not exist on server", clientfd);
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
            //printf("%s\n", ptr->versionNum);
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
    if(!isDirectoryExists(token))
    {
        sendMessage("er:", clientfd);
        return 0;
    }
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
    
    //printf("%s\n", response);
    sendMessage(response, clientfd);
    free(response);
}

int commit(char* token, int clientfd)
{
    token = &token[3];
    //printf("%s\n", token);
    if(!isDirectoryExists(token))
    {
        sendMessage("er:Project does not exist", clientfd);
        return 0;
    }
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
    if(clientResponse[0] == 'e' && clientResponse[1] == 'r')
    {
        return 0;
    }
    commitNodes = addCommitNode(commitNodes, token, id, clientResponse);
    //printf("%s\n", commitNodes->commit);
}

int push(char* token, int clientfd)
{
    //printf("Client sent: %s\n", &token[3]);
    token = &token[3];
    int index = 0;
    char* project_name_length = (char*)malloc(sizeof(char) * 1000);
    bzero(project_name_length, 1000);
    while (token[index] != ':')
    {
        project_name_length[index] = token[index];
        index++;
    }
    project_name_length[index] = '\0';
    int project_name_len = atoi(project_name_length);
    int i = 0;
    index++; // character after : so start of projectName
    char* projectName = (char*)malloc(sizeof(char) * ( project_name_len + 1) );
    bzero(projectName, project_name_len + 1);
    while (i < project_name_len)
    {
        projectName[i] = token[index];
        i++;
        index++;
    }
    projectName[i] = '\0';
    //printf("project name is %s\n", projectName);
    
    //check if project exists on server
    char* projectPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 2));
    bzero(projectPath, strlen(projectName) + 2 );
    strcpy(projectPath, projectName);
    strcat(projectPath, "/");

    if(!isDirectoryExists(projectPath))
    {
        //printf("Sending client: er:project does not exist on the server\n");
        sendMessage("er:project does not exist on the server", clientfd);
        return 0; 
    }

    // check if stored .Commit matches .Commit client just sent
    index++; // character after : so start of commit_length
    char* commit_length = (char*)malloc(sizeof(char) * 1000);
    bzero(commit_length, 1000);
    i = 0;
    while (token[index] != ':')
    {
        commit_length[i] = token[index];
        i++;
        index++;
    }
    commit_length[i] = '\0';
    int commit_len = atoi(commit_length);
    i = 0;
    index++; // character after : so start of commit contents
    char* clientCommit = (char*)malloc(sizeof(char) * (commit_len + 1));
    bzero(clientCommit, commit_len + 1);
    while(i < commit_len)
    {
        clientCommit[i] = token[index];
        i++;
        index++;
    }

    //printf("Client sent the commit: %s\n", clientCommit);
    
    // check hash of client commit
    char* client_commit_hash = getHash(clientCommit);

    // see if it matches with anything in the pending commit list on the server

    CommitNode* commitPtr = commitNodes; // head
    while(commitPtr != NULL)
    {    
        char* server_commit_hash = getHash(commitPtr->commit); 
        if(strcmp(client_commit_hash, server_commit_hash) == 0)
        {
            //printf("Matching .Commit found\n");
            break; // match found
        }
        commitPtr = commitPtr->next;
    }

    if(commitPtr == NULL) // list of .Commits on the server side is empty or there is no match with the .Commit the client just sent
    {
        //printf("Sending client: er:no match for .Commit was found\n");
        sendMessage("er:no match for .Commit was found", clientfd);   // error
        return 0; 
    }
    else 
    {
        //printf("Sending client: su:match for .Commit was found\n");
        sendMessage("su:match for .Commit was found", clientfd);   // success
    }
    
    char* received_files = readMessage(received_files, clientfd); // reading file contents sent from client 
    //printf("received_files is: %s\n", received_files);
    token = (char*)malloc(sizeof(char) * (strlen(received_files) + 1));
    bzero(token, strlen(received_files) + 1);
    strcpy(token, received_files);
    free(received_files);
    //printf("Message from client is %s\n", token);

    
    index = 0;

    // get numFiles
    char* num_of_files = (char*)malloc(sizeof(char)*100);
    bzero(num_of_files, 100);
    while(token[index] != ':')
    {
        num_of_files[index] = token[index];
        index++;
    }
    num_of_files[index] = '\0';
    int numFiles = atoi(num_of_files);
    //printf("Number of files client sent is: %d\n", numFiles);
    
    i = 0;
    int strIndex;
    index++; // skip : after numFiles

    // make FileNode list 
    FileNode* fileHead = NULL;

    while( i < numFiles) // go through rest of message (change malloc amounts later)
    {
        strIndex = 0;
        char* file_name_length = (char*)malloc(sizeof(char) * 100);
        bzero(num_of_files, 100);
        while(token[index] != ':')
        {
            file_name_length[strIndex] = token[index];
            index++;
            strIndex++;
        }
        file_name_length[strIndex] = '\0';
        index++; // skip : after file_name_length
        int file_name_len = atoi(file_name_length);
        strIndex = 0;
        char* file_name = (char*)malloc(sizeof(char) * (file_name_len + 1));
        bzero(file_name, file_name_len + 1);
        while(strIndex < file_name_len)
        {
            file_name[strIndex] = token[index];
            index++;
            strIndex++;    
        }
        file_name[strIndex] = '\0';
        index++; // skip : after file_name
        //printf("File name is: %s\n", file_name);
        strIndex = 0;
        char* file_length = (char*)malloc(sizeof(char) * 1000);
        bzero(file_length, 1000);
        while(token[index] != ':')
        {
            file_length[strIndex] = token[index];
            index++;
            strIndex++;
        }
        file_length[strIndex] = '\0';
        index++; // skip : after file_length
        int file_len = atoi(file_length);
        strIndex = 0;
        char* file_contents = (char*)malloc(sizeof(char) * (file_len + 1));
        bzero(file_contents, file_len + 1);
        while(strIndex < file_len)
        {
            file_contents[strIndex] = token[index];
            index++;
            strIndex++;
        }
        file_contents[strIndex] = '\0';
        //printf("File contents are: %s\n", file_contents);
        index++; // skip : after file_contents

        FileNode* tempfnode = (FileNode*)malloc(sizeof(FileNode));

        tempfnode->pathName = (char*)malloc(sizeof(char) * (file_name_len + 1));
        bzero(tempfnode->pathName, (file_name_len + 1));
        strcpy(tempfnode->pathName, file_name);

        tempfnode->contents = (char*)malloc(sizeof(char) * (strlen(file_contents) + 1));
        bzero(tempfnode->contents, (strlen(file_contents) + 1));
        strcpy(tempfnode->contents, file_contents);

        if (fileHead == NULL) // list is empty so assign as head
        {
            tempfnode->next = NULL;
            fileHead = tempfnode;
        }
        else // add to end of list
        {
            FileNode* tmp_ptr = fileHead;
            while(tmp_ptr->next != NULL)
            {
                tmp_ptr = tmp_ptr->next;
            } 
            tmp_ptr->next = tempfnode;
            tempfnode->next = NULL;   
        }
        i++; 

    }

    /*
    FileNode* printing_fptr = fileHead;
    while(printing_fptr != NULL)
    {
        printf("File pathName is: %s\n", printing_fptr->pathName);
        printf("File contents are: %s\n", printing_fptr->contents);
        printing_fptr = printing_fptr->next;
    }
    */
    
    
    //make manifest LL
    char* manifestPath = malloc(sizeof(char) * ( strlen(projectName) + 11));
    bzero(manifestPath, strlen(projectName) + 11);
    memcpy(manifestPath, projectName, strlen(projectName));
    strcat(manifestPath, "/");
    strcat(manifestPath, ".Manifest");
    int manifestFD = open(manifestPath, O_RDONLY);
    if(manifestFD == -1 )
    {
        //printf("Sending client: er: No manifest found, push failed");
        sendMessage("er: No manifest found, push failed", clientfd);
        return 0;
    }
    char* manifestContents = (char*)malloc(sizeof(char) * 50);
    manifestContents = readFile(manifestContents, manifestFD);
    node* manifestHead = manifest_to_LL(manifestContents);
    close(manifestFD);

    //printf("Manifest contents are: %s\n", manifestContents);

    
    // make .Commit to LL
    node* commitHead = manifest_to_LL(clientCommit); // commitHead versionNum = clientid

    // expire pending commits
    /*
    commitPtr = commitNodes;
    while(commitPtr != NULL)
    {
        CommitNode* tempcnode = commitPtr;
        commitPtr = commitPtr->next;
        free(tempcnode);
    }
    */

    commitNodes = expirePendingCommits(commitNodes, projectName);

    // tar up project in the same project folder in a subdirectory called backups
    char* old_manifest_version = (char*)malloc(sizeof(char) * (strlen(manifestHead->versionNum) + 1));
    bzero(old_manifest_version, strlen(manifestHead->versionNum) + 1);
    strcpy(old_manifest_version, manifestHead->versionNum);
    //printf("Old manifest version is: %s\n", old_manifest_version);
    char* backupPath = (char*)malloc(sizeof(char) * ( strlen(projectName) + 10 ) );
    bzero(backupPath, strlen(projectName) + 10);
    strcpy(backupPath, projectName);
    strcat(backupPath, "/");
    strcat(backupPath, "backups/");
    
    /*
    if(!isDirectoryExists(backupPath))
    {
        printf("Creating backups directory\n");
        char* command = (char*)malloc(sizeof(char) * (strlen(backupPath) + 7 ));
        bzero(command, strlen(backupPath) + 7);
        sprintf(command, "mkdir %s", backupPath);
        system(command);   
    }
    */
   
    //printf("Backup path is: %s\n" , backupPath);
    makePath(backupPath);
    
    char* tar_path_1 = (char*)malloc(sizeof(char) * (strlen(projectName) + 16 + strlen(manifestHead->versionNum)));
    bzero(tar_path_1, strlen(projectName) + 16 + strlen(manifestHead->versionNum));
    strcpy(tar_path_1, projectName);
    strcat(tar_path_1, "/backups/");
    char* tar_project = (char*)malloc(sizeof(char) * (strlen(manifestHead->versionNum) + 5));
    bzero(tar_project, strlen(manifestHead->versionNum) + 5);
    strcpy(tar_project, manifestHead->versionNum);
    strcat(tar_project, ".tar");
    strcat(tar_path_1, tar_project); 
    char* tar_path_2 = (char*)malloc(sizeof(char) * (strlen(projectName) + 2));
    bzero(tar_path_2, strlen(projectName) + 2 );
    strcpy(tar_path_2, projectName);
    strcat(tar_path_2, "/");
    char* cmd = (char*)malloc(sizeof(char) * (strlen(tar_path_1) + strlen(tar_path_2) + 11));
    bzero(cmd, strlen(tar_path_1) + strlen(tar_path_2) + 11);
    sprintf(cmd, "tar -cvf %s %s", tar_path_1, tar_path_2);
    //printf("Tar command is: %s\n", cmd);
    
    system(cmd);
    
    // go through .Commit and make changes accordingly
    node* cmt_ptr = commitHead;
    while(cmt_ptr != NULL)
    {
        if(cmt_ptr == commitHead) // can skip clientid
        {
            cmt_ptr = cmt_ptr->next;
            continue;
        }
        else 
        {
            //printf("cmt_ptr->status is %s\n", cmt_ptr->status); // delete later
            if(strcmp(cmt_ptr->status, "A") == 0) // add file to project directory 
            {
                // write creates new file in project directory
                makePath(cmt_ptr->pathName);
                int new_file_fd = open(cmt_ptr->pathName, O_RDWR | O_CREAT, 00666);
                char* to_write = search_in_fileLL(fileHead, cmt_ptr->pathName); 
                //printf("File to be added has contents: %s\n", to_write);
                if(to_write == NULL)
                {
                    
                    //printf("Sending client: er:file not found in linked list\n");
                    untar_project(projectName, old_manifest_version);
                    sendMessage("er:file not found in linked list", clientfd);
                    return 0;
                }
                write(new_file_fd, to_write, strlen(to_write));   

                // make new manifest node
                
                node* add_to_man = (node*)malloc(sizeof(node));
                add_to_man = nullNode(add_to_man);
                strcpy(add_to_man->status, "-");
                strcpy(add_to_man->pathName, cmt_ptr->pathName);
                strcpy(add_to_man->versionNum, "0");
                strcpy(add_to_man->hash, getHash(to_write)); 
                add_to_man->next = NULL;

                node* manPtr = manifestHead;
                while(manPtr->next != NULL)
                {
                    manPtr = manPtr->next;
                }
                manPtr->next = add_to_man;
                //printf("Added entry to manifest\n");
                node* printing = manifestHead;
                while(printing != NULL)
                {
                    //printf("Manifest has: %s\n", printing->pathName);
                    printing = printing->next;
                }

                close(new_file_fd);
            }   

            else if(strcmp(cmt_ptr->status, "D") == 0) // remove file from project directory
            {
                // removes file from project
                char* remove_cmd = (char*)malloc(sizeof(char) * ( strlen(cmt_ptr->pathName)+ 7 ));
                bzero(remove_cmd, strlen(cmt_ptr->pathName)+ 7);
                sprintf(remove_cmd, "rm -r %s", cmt_ptr->pathName);
                system(remove_cmd);

                // remove node from manifest
                manifestHead = remove_from_manLL(manifestHead, cmt_ptr->pathName);
                //printf("Removed entry from manifest\n");

                node* printing = manifestHead;
                while(printing != NULL)
                {
                    //printf("Manifest has: %s\n", printing->pathName);
                    printing = printing->next;
                }
            }

            else if(strcmp(cmt_ptr->status, "M") == 0) // overwrite file in project directory
            {
                // modify existing file
                makePath(cmt_ptr->pathName);
                int fileFD = open(cmt_ptr->pathName, O_RDWR | O_CREAT | O_TRUNC, 00666);
                if(fileFD == -1)
                {
                    //printf("Sending client: er:file not found in project\n");
                    untar_project(projectName, old_manifest_version);
                    sendMessage("er:file not found in project", clientfd);
                    return 0;
                }
                char* to_write = search_in_fileLL(fileHead, cmt_ptr->pathName); 
                if(to_write == NULL)
                {
                    //printf("Sending client: er:file not found in linked list\n");
                    untar_project(projectName, old_manifest_version);
                    sendMessage("er:file not found in linked list", clientfd);
                    close(fileFD);
                    return 0;
                }
                write(fileFD, to_write, strlen(to_write));

                // modify manifest entry
                manifestHead = update_manifest_node(manifestHead, cmt_ptr, to_write);
                close(fileFD);
            } 

            cmt_ptr = cmt_ptr->next; 

        }
        
    }
    
    //printf("Updating manifest now\n");
    // increment manifest version

    int new_man_ver = atoi(manifestHead->versionNum);
    new_man_ver++;
    char* new_man_version = int_to_string(new_man_ver);
    strcpy(manifestHead->versionNum, new_man_version);

    // overwrite manifest
    manifestFD = open(manifestPath, O_RDWR | O_CREAT | O_TRUNC, 00600 );
    if(manifestFD == -1)
    {
        //printf("Sending client: er:Manifest does not exist on server\n");
        untar_project(projectName, old_manifest_version);
        sendMessage("er:Manifest does not exist on server", clientfd);
        return 0;
    }
    LL_to_manifest(manifestHead, manifestFD); // base .Manifest on LL after changes are made
    close(manifestFD);

    freeList(manifestHead);
    //freeList(commitHead);
    free_fileLL(fileHead);
    //free(clientCommit);
    
    manifestFD = open(manifestPath, O_RDONLY);
    if(manifestFD == -1 )
    {
        //printf("Sending client: er: No manifest found, push failed\n");
        untar_project(projectName, old_manifest_version);
        sendMessage("er: No manifest found, push failed", clientfd);
        return 0;
    } 

    // Create or add to .History based on .Commit LL
    addToHistory(commitHead, old_manifest_version, projectName);


    freeList(commitHead);
    manifestContents = readFile(manifestContents, manifestFD); // new contents
    char* manifest_len = int_to_string(strlen(manifestContents));
    char* finalMessage = (char*)malloc(sizeof(char) * ( strlen(manifest_len) + strlen(manifestContents) + 5));
    bzero(finalMessage, strlen(manifest_len) + strlen(manifestContents) + 5);
    strcat(finalMessage, "su:");
    strcat(finalMessage, manifest_len);
    strcat(finalMessage, ":");
    strcat(finalMessage, manifestContents);
    close(manifestFD);
    //printf("The finalMessage is: %s\n", finalMessage);
    sendMessage(finalMessage, clientfd);
    //free(finalMessage);
    return 0;
    
}

int history(char* token, int clientfd)
{
    //printf("Client sent %s\n", &token[3]);
    token = &token[3];  
    int index = 0;
    char* project_name_length = (char*)malloc(sizeof(char) * 100);
    bzero(project_name_length, 100);
    while(token[index] != ':')
    {
        project_name_length[index] = token[index];
        index++;    
    }  
    project_name_length[index] = '\0';
    index++; // skip : after project_name_length

    int project_name_len = atoi(project_name_length);
    int strIndex = 0;
    char* projectName = (char*)malloc(sizeof(char) * (project_name_len + 1));
    bzero(projectName, project_name_len + 1);
    while(strIndex < project_name_len)
    {
        projectName[strIndex] = token[index];
        strIndex++;
        index++;
    }
    projectName[strIndex] = '\0';

    char* projectPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 2));
    bzero(projectPath, strlen(projectName) + 2 );
    strcpy(projectPath, projectName);
    strcat(projectPath, "/");

    if(!isDirectoryExists(projectPath))
    {
        //printf("Project could not be found on the server");
        sendMessage("er: Project could not be found on the server", clientfd);
        return 0;
    }

    char* historyPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 10 ));
    bzero(historyPath, strlen(projectName) + 10);
    strcpy(historyPath, projectName);
    strcat(historyPath, "/.History");

    int historyFD = open(historyPath, O_RDONLY);
    char* historyContents = readFile(historyContents, historyFD);
    char* history_length = int_to_string(strlen(historyContents));
    char* finalMessage = (char*)malloc(sizeof(char) * (strlen(historyContents) + strlen(history_length) + 5));
    bzero(finalMessage, strlen(historyContents) + strlen(history_length) + 5);
    strcpy(finalMessage, "su:");
    strcat(finalMessage, history_length);
    strcat(finalMessage, ":");
    strcat(finalMessage, historyContents);
    close(historyFD);
    //printf("Sending client: %s\n", finalMessage);
    sendMessage(finalMessage, clientfd);
    
}

int rollback(char* token, int clientfd)
{
    //printf("Client sent %s\n", &token[3]);
    token = &token[3];

    int index = 0;
    char* project_name_length = (char*)malloc(sizeof(char) * 100);
    bzero(project_name_length, 100);
    while(token[index] != ':')
    {
        project_name_length[index] = token[index];
        index++;    
    }  
    project_name_length[index] = '\0';
    index++; // skip : after project_name_length

    int project_name_len = atoi(project_name_length);
    int strIndex = 0;
    char* projectName = (char*)malloc(sizeof(char) * (project_name_len + 1));
    bzero(projectName, project_name_len + 1);
    while(strIndex < project_name_len)
    {
        projectName[strIndex] = token[index];
        strIndex++;
        index++;
    }
    projectName[strIndex] = '\0';
    index++; // skip : after project name
    char* projectPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 2));
    bzero(projectPath, strlen(projectName) + 2 );
    strcpy(projectPath, projectName);
    strcat(projectPath, "/");

    if(!isDirectoryExists(projectPath))
    {
        //printf("Project could not be found on the server");
        sendMessage("er: Project could not be found on the server", clientfd);
        return 0;
    }

    strIndex = 0;
    char* version_length = (char*)malloc(sizeof(char) * 100);
    bzero(version_length, 100);
    while(token[index] != ':')
    {
        version_length[strIndex] = token[index];
        strIndex++;
        index++;
    }
    version_length[strIndex] = '\0';
    int version_len = atoi(version_length);
    //printf("version_len is %d\n", version_len);
    index++; // skip : after version name length
    strIndex = 0;
    char* projectVersion = (char*)malloc(sizeof(char) * (version_len + 1));
    bzero(projectVersion, version_len + 1);
    while(strIndex < version_len)
    {
        projectVersion[strIndex] = token[index];
        strIndex++;
        index++;
    }
    projectVersion[strIndex] = '\0';

    free(project_name_length);
    free(version_length);
    //printf("Project name is: %s\n", projectName);
    //printf("Project version is: %s\n", projectVersion);

    if(canRollback(projectName, projectVersion) == 0)
    {
        //printf("Can not rollback to version %s\n", projectVersion);
        sendMessage("er: Can not rollback to version greater than or equal to current project version on the server", clientfd);
    }
    else
    {
        untar_project(projectName, projectVersion);
        if(atoi(projectVersion) == 0) // delete backups folder
        {
            deleteBackups(projectName);
        }
        sendMessage("su:Rollback successful", clientfd);
    }
    
}

int update(char* token, int clientfd)
{
    token = &token[3];

    if(!isDirectoryExists(token))
    {
        sendMessage("er:Project not on server", clientfd);
        return 0;
    }

    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(token) + 12));
    bzero(manifestPath, strlen(token) + 12);
    sprintf(manifestPath, "%s/.Manifest", token);
    int manifestFD = open(manifestPath, O_RDONLY);
    char* manifestContents = readFile(manifestContents, manifestFD);
    sendMessage(manifestContents, clientfd);
    close(manifestFD);
    
    free(manifestContents);
}

int upgrade(char* token, int clientfd)
{
    //printf("Client sent %s\n", &token[3]);
    token = &token[3];  
    int index = 0;
    char* project_name_length = (char*)malloc(sizeof(char) * 100);
    bzero(project_name_length, 100);
    while(token[index] != ':')
    {
        project_name_length[index] = token[index];
        index++;    
    }  
    project_name_length[index] = '\0';
    index++; // skip : after project_name_length

    int project_name_len = atoi(project_name_length);
    int strIndex = 0;
    char* projectName = (char*)malloc(sizeof(char) * (project_name_len + 1));
    bzero(projectName, project_name_len + 1);
    while(strIndex < project_name_len)
    {
        projectName[strIndex] = token[index];
        strIndex++;
        index++;
    }
    projectName[strIndex] = '\0';

    char* projectPath = (char*)malloc(sizeof(char) * (strlen(projectName) + 2));
    bzero(projectPath, strlen(projectName) + 2 );
    strcpy(projectPath, projectName);
    strcat(projectPath, "/");

    if(!isDirectoryExists(projectPath))
    {
        //printf("Project could not be found on the server");
        sendMessage("er: Project could not be found on the server", clientfd);
        free(projectPath);
        return 0;
    }

    sendMessage("su: Project exists on the server", clientfd);

    char* clientRequest = readMessage(clientRequest, clientfd);
    
    strIndex = 0;
    char* numFiles = (char*) malloc(sizeof(char));
    numFiles[strIndex] = '\0';

    while(clientRequest[strIndex] != ':')
    {
        numFiles[strIndex] = clientRequest[strIndex];
        char* tmp = (char*) realloc(numFiles, strIndex+2);
        numFiles = tmp;
        strIndex++;
    }

    int files = atoi(numFiles);
    free(numFiles);

    int i = 0;
    strIndex++;
    FileNode* fileRoot = NULL;

    for(i = 0; i < files; i++)
    {
        int pathIndex = 0;
        char* filePathLenStr = (char*) malloc(sizeof(char));
        filePathLenStr[0] = '\0';

        while(clientRequest[strIndex] != ':')
        {
            filePathLenStr[pathIndex] = clientRequest[strIndex];
            char* tmp = (char*) realloc(filePathLenStr, pathIndex+2);
            filePathLenStr = tmp;
            strIndex++;
            pathIndex++;
        }

        int filePathLen = atoi(filePathLenStr);
        free(filePathLenStr);
        strIndex++;

        int j = 0;
        char* filePath = (char*) malloc(sizeof(char) * (filePathLen + 1));
        bzero(filePath, filePathLen+1);

        for(j = 0; j < filePathLen; j++)
        {
            filePath[j] = clientRequest[strIndex];
            strIndex++;
        }

        int fd = open(filePath, O_RDONLY);
        char* fileContents = readFile(fileContents, fd);
        int fileSize = getFileSize(filePath);
        close(fd);

        if(fileRoot == NULL)
        {
            fileRoot = (FileNode*) malloc(sizeof(FileNode));
            fileRoot->pathName = filePath;
            fileRoot->contents = fileContents;
            fileRoot->size = fileSize;
            fileRoot->next = NULL;
        }
        else
        {
            FileNode* tmpFile = (FileNode*) malloc(sizeof(FileNode));
            tmpFile->pathName = filePath;
            tmpFile->contents = fileContents;
            tmpFile->size = fileSize;
            tmpFile->next = fileRoot;
            fileRoot = tmpFile;
        }

    }

    char* manifestPath = (char*) malloc(sizeof(char) * (strlen(projectName) + 12));
    bzero(manifestPath, strlen(projectName) + 12);
    sprintf(manifestPath, "%s/.Manifest", projectName);

    int manifestFD = open(manifestPath, O_RDONLY);
    char* manifestContents = readFile(manifestContents, manifestFD);
    close(manifestFD);
    FileNode* tmpFile = (FileNode*) malloc(sizeof(FileNode));
    tmpFile->pathName = manifestPath;
    tmpFile->contents = manifestContents;
    tmpFile->size = strlen(manifestContents);
    tmpFile->next = fileRoot;
    fileRoot = tmpFile;
    
    files++;
    int msgLen = 0;
    msgLen += numDigits(files) + 1;
    FileNode* ptr = fileRoot;

    while(ptr != NULL)
    {
        int pathLen = strlen(ptr->pathName);
        msgLen += numDigits(pathLen) + 1 + pathLen + 1;
        int dataLen = strlen(ptr->contents);
        msgLen += numDigits(dataLen) + 1 + dataLen + 1;
        ptr = ptr->next;
    }

    char* message = (char*) malloc(sizeof(char) * (msgLen) + 3);
    bzero(message, msgLen + 3);
    ptr = fileRoot;

    strcat(message, int_to_string(files));
    strcat(message, ":");

    while(ptr != NULL)
    {
        int pathLen = strlen(ptr->pathName);
        strcat(message, int_to_string(pathLen));
        strcat(message, ":");
        strcat(message, ptr->pathName);
        strcat(message, ":");
        int dataLen = strlen(ptr->contents);
        strcat(message, int_to_string(dataLen));
        strcat(message, ":");
        strcat(message, ptr->contents);
        strcat(message, ":");
        ptr = ptr->next;
    }

    sendMessage(message, clientfd);

    free(message);
    free(clientRequest);
    free(projectPath);
}

void* clientComm(void* args)
{
    pthread_mutex_lock(&thread_lock);
    int fd = *((int*) args);  //dereference clientfd from args
    char* buffer = readMessage(buffer, fd);
    //printf("%s\n", buffer);
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
    else if(strcmp(tokens, "pu") == 0)
        push(buffer, fd);
    else if(strcmp(tokens, "hs") == 0)
        history(buffer, fd);
    else if(strcmp(tokens, "ro") == 0)
        rollback(buffer, fd);
    else if(strcmp(tokens, "ud") == 0)
        update(buffer, fd);
    else if(strcmp(tokens, "ug") == 0)
        upgrade(buffer, fd);

    printf("SERVER: Client: %d disconnected\n", fd);
    close(fd);

    free(buffer);
    
    pthread_mutex_unlock(&thread_lock);
    pthread_exit(NULL);
}

void signal_handler(int signum)
{
    thread_node* ptr = threadFront;
    int i = 0;
    while(ptr != NULL)
    {
        pthread_join(ptr->thread_id, NULL);
        printf("Thread: %d removed\n", i);
        i++;
        thread_node* temp = ptr;
        ptr = ptr->next;
        free(temp);
    }
    printf("Server has shut down\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    clientIds = 0;
    commitNodes = NULL;
    pthread_mutex_init(&thread_lock, NULL);
    signal(SIGINT, signal_handler);    

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

    threadFront = NULL;

    while(1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
          error("ERROR on accept");
        else
        {
            thread_node* temp = (thread_node*) malloc(sizeof(thread_node));
            temp->next = threadFront;
            threadFront = temp;

            if(pthread_create(&temp->thread_id, NULL, clientComm, &newsockfd) != 0)
                printf("Cannot create thread\n");
            else
                printf("SERVER: Connected to Client: %d\n", newsockfd);
        }
    }

    pthread_exit(NULL); 
     
    return 0; 
}