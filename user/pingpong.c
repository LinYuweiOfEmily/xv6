#include "kernel/types.h"
#include "user.h"


/**
 * 
 * 1.父进程向管道中写入数据，子进程从管道将其读出并打印<pid>: received ping from pid <father pid> 
 * ，其中<pid>是子进程的进程ID，<father pid>是父进程的进程ID。
 * 2.子进程从父进程收到数据后，通过写入另一个管道向父进程传输数据，
 * 然后由父进程从该管道读取并打印<pid>: received pong from pid <child pid>，其中<pid>是父进程的进程ID, <child pid>是子进程的进程ID。
 */


int main(int argc, char* argv[]) {

    //创建两个管道
    int f2c[2];         //父向子
    
    int childPid;

    //创建管道
    pipe(f2c);


    if ((childPid = fork()) == 0) {
        int sonPid = getpid();
        //子进程
        int parentPid;
        // printf("%d:1\n", sonPid);
        close(f2c[1]);
        // printf("%d:2\n", sonPid);

        if (read(f2c[0], &parentPid, sizeof(int)) <= 0) {
            printf("Child: Error reading from pipe\n");
            exit(1);
        }
        // printf("%d:3\n", sonPid);

        close(f2c[0]);
        printf("%d: received ping from pid %d\n", sonPid, parentPid);



    } else {
        //父进程
        int parentPid = getpid();
        
        // printf("%d:1\n", parentPid);
        close(f2c[0]);
        // printf("%d:2\n", parentPid);


        // 向子进程发送数据
        if (write(f2c[1], &parentPid, sizeof(int)) <= 0) {
            printf("Parent: Error writing to pipe\n");
            exit(1);
        }


        // printf("%d:4\n", parentPid);
        close(f2c[1]);

        wait(0);
        
        printf("%d: received pong from pid %d\n", parentPid, childPid);
        
    }
    exit(0);
}