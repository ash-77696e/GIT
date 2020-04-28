#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct node
{
    char* status;
    char* pathName;
    char* versionNum;
    unsigned char* hash;
    struct node* next;
} node;

typedef struct FileNode
{
    char* pathName;
    char* contents;
    int size;
    struct FileNode* next;
} FileNode;

#endif