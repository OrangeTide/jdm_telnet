/* macros.h - provides some general purpose macros
 * PUBLIC DOMAIN 2005 - Jon Mayo
 */
#ifndef MACROS_H
#define MACROS_H
#include <stdio.h>
#include <string.h>

#define FAIL(s) do { perror(s); exit(EXIT_FAILURE); } while(0)

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
# define VERBOSE(...) fprintf(stderr, __VA_ARGS__)
# ifdef NTRACE
#  define TRACE(...) /* trace disabled */
# else
#  define TRACE(...) fprintf(stderr, __VA_ARGS__)
# endif
#else
# error Requires C99.
#endif

#define NR(x) (sizeof(x)/sizeof*(x))

#ifndef NDEBUG 
/* initialize with junk */
# define JUNKINIT(ptr, len) memset((ptr), 0x99, (len));
#else
# define JUNKINIT(ptr, len) /* do nothing */
#endif

#endif

