typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef uint pde_t;

// _STDIO_H가 정의되어 있지 않다면 off_t를 int로 정의
#ifndef _STDIO_H
typedef int off_t;
#endif
