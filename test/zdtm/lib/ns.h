#ifndef __ZDTM_NS__
#define __ZDTM_NS__

#include "lock.h"

extern futex_t sig_received;
extern char *pidfile;

extern void ns_create(int argc, char **argv);
extern int ns_init(int argc, char **argv);

extern void test_waitsig(void);
extern void parseargs(int, char **);

enum {
	TEST_NO_SIG = 0,
	TEST_SIG_PRE,
	TEST_SIG_STOP,
};

#endif
