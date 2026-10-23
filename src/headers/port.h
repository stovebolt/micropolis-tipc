#ifndef PORT_H
#define PORT_H
/* QUAD must be exactly 32 bits on every build (original Mac "long"). */
#if defined(__WATCOMC__) || defined(MSDOS)
#define QUAD long
#else
#define QUAD int
#endif
#if defined(__WATCOMC__)
#define FAR __far
#define NEAR __near
#else
#define FAR
#define NEAR
#endif
typedef unsigned char Byte;
typedef Byte * Ptr;
typedef char **Handle;
extern Ptr NewPtr();
#endif
