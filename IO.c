#include "structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

char* readFile(char* buffer, int fd)
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

int isDirectoryExists(const char* path) 
{
    printf("Path being checked is: %s\n", path); // delete later
    struct stat stats;
    stat(path, &stats);

    if(S_ISDIR(stats.st_mode))
        return 1;
    
    return 0;
}

int getFileSize(char* path)
{
    int fd = open(path, O_RDONLY);
    int size = lseek(fd, 0, SEEK_END);
    close(fd);
    return size;
}

