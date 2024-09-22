#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    int fd;        // 파일 디스크립터
    off_t offset;  // 파일 오프셋
    char buf[512]; // 파일 내용을 저장할 버퍼

    // 입력받은 인자가 4개가 아닌 경우 사용법 출력
    if (argc < 4)
    {
        printf(1, "usage: lseek_test <filename> <offset> <string>\n");
        exit();
    }

    // open() 시스템 콜을 호출하여 파일을 읽기/쓰기 모드로 오픈
    fd = open(argv[1], O_CREATE | O_RDWR);
    // open() 함수가 실패할 경우 에러 메시지 출력 후 프로그램 종료
    if (fd < 0)
    {
        printf(2, "lseek_test: cannot open %s\n", argv[1]);
        exit();
    }

    // read() 시스템 콜을 호출하여 버퍼에 파일 내용을 읽어옴
    offset = read(fd, buf, sizeof(buf));
    // read() 함수가 실패할 경우 에러 메시지 출력 후 프로그램 종료
    if (offset < 0)
    {
        printf(2, "lseek_test: read error\n");
        exit();
    }

    // 버퍼에 저장된 파일 내용 출력
    printf(1, "Before : %s\n", buf);

    // atoi() 함수를 호출하여 offset 인자를 정수로 변환
    offset = atoi(argv[2]);

    // lseek() 시스템 콜을 호출하여 파일 오프셋을 지정한 offset으로 이동시킴
    // lseek() 함수가 실패할 경우 에러 메시지 출력 후 프로그램 종료
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        printf(2, "lseek_test: lseek error\n");
        exit();
    }

    // write() 시스템 콜을 호출하여 인자로 받은 문자열을 파일에 씀
    // write() 함수가 실패할 경우 에러 메시지 출력 후 프로그램
    if (write(fd, argv[3], strlen(argv[3])) < 0)
    {
        printf(2, "lseek_test: write error\n");
        exit();
    }

    // lseek() 시스템 콜을 호출하여 파일 오프셋을 파일의 시작으로 이동시킴
    // lseek() 함수가 실패할 경우 에러 메시지 출력 후 프로그램 종료
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        printf(2, "lseek_test: lseek error\n");
        exit();
    }

    // read() 시스템 콜을 호출하여 버퍼에 파일 내용을 읽어옴
    // read() 함수가 실패할 경우 에러 메시지 출력 후 프로그램 종
    offset = read(fd, buf, sizeof(buf));
    if (offset < 0)
    {
        printf(2, "lseek_test: read error\n");
        exit();
    }

    // 버퍼에 저장된 파일 내용 출력
    printf(1, "After : %s\n", buf);
    // 파일 디스크립터를 닫음
    close(fd);
    // 프로그램 종료
    exit();
}