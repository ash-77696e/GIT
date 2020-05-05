#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

int main(int argc, char* argv)
{
    pid_t pid;
    pid = fork(); //fork client

    if(pid == 0)
    {
        system("cd client && ./WTF create p1"); //should fail
        sleep(1);
        system("cd client && ./WTF configure $HOSTNAME 7990"); //success
        sleep(1);
        system("cd client && ./WTF create p1"); //sucess on both sides
        sleep(1);
        system("cd client && touch p1/test.txt && echo \"Hello there!\" > p1/test.txt"); //create file in p1 and add text
        sleep(1);
        system("cd client && ./WTF add p1 test.txt"); //add to manifest
        sleep(1);
        system("cd client && ./WTF commit p1"); //commit success
        sleep(1);
        system("cd client && ./WTF push p1"); //push success
        sleep(1);
        system("cd client && ./WTF currentversion p1"); //current version with 1: p1/test.txt 0
        sleep(1);
        system("cd client && touch p1/test2.txt && echo \"Goodbye!\" > p1/test2.txt"); //create new file
        sleep(1);
        system("cd client && ./WTF add p1 test2.txt"); //add this file
        sleep(1);
        system("cd client && ./WTF remove p1 test.txt"); //remove first file
        sleep(1);
        system("cd client && ./WTF commit p1"); //success
        sleep(1);
        system("cd client && ./WTF push p1"); //success, but should only have 1 file
        sleep(1);
        system("cd client && ./WTF currentversion p1"); //2: p1/test2.txt 0
        sleep(1);
        system("cd client && ./WTF history p1");
        sleep(1);
        system("cd client && ./WTF update p1"); //should print Up to Date
        sleep(1);
        system("cd client && ./WTF upgrade p1"); //should fail because of blank update file (Up to Date)
        sleep(1);
        system("cd client && ./WTF rollback p1 0"); //rollback server project to version 0 (empty)
        sleep(1);
        system("cd client && rm -r p1"); //remove p1 from client
        sleep(1);
        system("cd client && ./WTF checkout p1"); //server and client now have the same empty project
        sleep(1);
        system("cd client && ./WTF"); //invalid number of arguments
        sleep(1);
        printf("You have passed all of the tests!\n"); //in the end server should only have test2.txt
        sleep(1);
        kill(pid, SIGINT);
    }
    else //server
    {
        chdir("server");
        system("./WTFserver 7990");
        wait(pid);
    }
}