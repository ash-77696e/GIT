#ifndef IO_H
#define IO_H

char* readFile(char* buffer, int fd);
int isDirectoryExists(const char* path);
int getFileSize(char* path);

#endif