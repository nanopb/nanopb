#include <stdio.h>

#ifdef UNITTESTS_SHORT_MSGS
/* Short debug messages for platforms with limited memory */
#define COMMENT(x) printf("\n----" x "----\n");
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "FAIL: Line %d\n", __LINE__); \
        status = 1; \
    } else { \
        printf("OK: Line %d\n", __LINE__); \
    }

#else

/* Elaborate debug messages for normal development */
#define COMMENT(x) printf("\n----" x "----\n");
#define STR(x) #x
#define STR2(x) STR(x)
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m " __FILE__ ":" STR2(__LINE__) " " #x "\n"); \
        status = 1; \
    } else { \
        printf("\033[32;1mOK:\033[22;39m " #x "\n"); \
    }
#endif

