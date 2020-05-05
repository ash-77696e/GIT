#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

int main(int argc, char* argv)
{
    pid_t pid;
    pid = fork();

    if(pid == 0)
    {
        system("cd server && ./WTFserver 7990");
    }
    else
    {
        chdir("./client");
        sleep(1);
        system("./WTF configure cp.cs.rutgers.edu 7990");
        sleep(1);
        system("./WTF create p1");
        sleep(1);
        system("touch p1/test.txt && echo \"Hello there!\" > p1/test.txt");
        sleep(1);
        system("./WTF add p1 test.txt");
        sleep(1);
        system("./WTF commit p1");
        sleep(1);
        system("./WTF push p1");
        sleep(1);
        system("./WTF currentversion p1");
        sleep(1);
        system("touch p1/test2.txt && echo \"Goodbye!\" > p1/test2.txt");
        sleep(1);
        system("./WTF add p1 test2.txt");
        sleep(1);
        system("./WTF remove p1 test.txt");
        sleep(1);
        system("./WTF commit p1");
        sleep(1);
        system("./WTF push p1");
        sleep(1);
        system("./WTF history p1");
        sleep(1);
        system("./WTF rollback p1 0");
        sleep(1);
        system("./WTF update p1");
        sleep(1);
        kill(pid, SIGKILL);
        printf("You have passed all of the tests!\n");
        wait();
    }
    
}