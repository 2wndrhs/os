#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    int fd;
    off_t offset;
    char buf[512];

    if (argc < 4)
    {
        printf(1, "usage: lseek_test <filename> <offset> <string>\n");
        exit();
    }

    fd = open(argv[1], O_CREATE | O_RDWR);
    if (fd < 0)
    {
        printf(2, "lseek_test: cannot open %s\n", argv[1]);
        exit();
    }

    offset = read(fd, buf, sizeof(buf));
    if (offset < 0)
    {
        printf(2, "lseek_test: read error\n");
        exit();
    }

    printf(1, "Before : %s\n", buf);

    offset = atoi(argv[2]);
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        printf(2, "lseek_test: lseek error\n");
        exit();
    }

    if (write(fd, argv[3], strlen(argv[3])) < 0)
    {
        printf(2, "lseek_test: write error\n");
        exit();
    }

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        printf(2, "lseek_test: lseek error\n");
        exit();
    }

    offset = read(fd, buf, sizeof(buf));
    if (offset < 0)
    {
        printf(2, "lseek_test: read error\n");
        exit();
    }

    printf(1, "After : %s\n", buf);

    close(fd);
    exit();
}