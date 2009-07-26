#ifndef _INTTYPES_H
#define _INTTYPES_H

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
typedef int64_t int64;
typedef uint64_t uint64;

typedef int ssize_t;
typedef int mode_t;

#include <direct.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <io.h>
#include <stdlib.h>
#define HAVE__MKDIR 1
#define HAVE_MEMSET 1
#define EMPTY_ARRAY_SIZE 1

#define PSS "\\"
#define PATH_MAX _MAX_PATH

#define LSB_FIRST
#define _MSC_VER_ICKY_TYPES
#pragma warning( disable: 4996 ) //disable "The POSIX name for this item is deprecated"
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
#define fstat _fstat
#define close _close
#define open _open
#define lseek _lseek
#define read _read


#ifdef __cplusplus
#define INLINE inline
#else
#define INLINE _inline
#define inline _inline
#endif

#endif
