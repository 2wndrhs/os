typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef uint pde_t;

// _OFF_T가 정의되어 있지 않다면 off_t를 int로 정의
#ifndef _OFF_T
typedef int off_t;
#endif
