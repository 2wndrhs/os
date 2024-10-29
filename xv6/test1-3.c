#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    printf(1, "start scheduler_test\n");

    int pid;
    int child_count = 3; // 생성할 자식 프로세스 수

    for (int i = 0; i < child_count; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed\n");
            exit();
        }

        if (pid == 0)
        {
            // Child process
            if (set_proc_info(2, 0, 0, 0, 300) < 0)
            {
                printf(1, "set_proc_info failed\n");
                exit();
            }

            while (1)
            {
            }
        }
    }

    // wait for all child processes to finish
    for (int i = 0; i < child_count; i++)
    {
        wait();
    }

    printf(1, "end of scheduler_test\n");

    exit();
}