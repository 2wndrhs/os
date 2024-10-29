#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    printf(1, "start scheduler_test\n");

    int pid = fork();
    if (pid < 0)
    {
        printf(1, "fork failed\n");
        exit();
    }

    if (pid == 0)
    {
        // Child process
        if (set_proc_info(1, 0, 0, 0, 500) < 0)
        {
            printf(1, "set_proc_info failed\n");
            exit();
        }

        while (1)
        {
        }
    }
    else
    {
        // wait for child process to finish
        wait();
    }

    printf(1, "end of scheduler_test\n");
    exit();
}