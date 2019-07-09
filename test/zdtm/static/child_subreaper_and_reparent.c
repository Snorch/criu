#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "zdtmtst.h"
#include "lock.h"

const char *test_doc	= "Check that child subreaper does not affect reparenting";
const char *test_author	= "Pavel Tikhomirov <ptikhomirov@virtuozzo.com>";

enum {
	TEST_FORK,
	TEST_SAVE,
	TEST_CRIU,
	TEST_CHECK,
	TEST_EXIT,
};

static futex_t *fstate;
static int *parent_before_cr;
static int *parent_after_cr;

enum {
	MAIN,
	SUBREAPER,
	HELPER,
	ORPHAN,
};

int worker(int type) {
	int pid, ret, status;

	switch (type) {
		case MAIN:
			setsid();

			pid = fork();
			if (pid < 0) {
				pr_perror("Failed to fork");
				exit(1);
			} else if (pid == 0) {
				exit(worker(SUBREAPER));
			}

			/* Wait until ORPHAN is ready to C/R */
			futex_wait_until(fstate, TEST_CRIU);

			test_daemon();
			test_waitsig();

			/* Give controll to ORPHAN to check it's parent */
			futex_set_and_wake(fstate, TEST_CHECK);
			futex_wait_until(fstate, TEST_EXIT);

			/* Cleanup */
			while (wait(&status) > 0) {
				if (!WIFEXITED(status) || WEXITSTATUS(status)) {
					fail("Wrong exit status: %d", status);
					return 1;
				}
			}

			if (*parent_before_cr != *parent_after_cr)
				fail("Parent missmatch before %d after %d", *parent_before_cr, *parent_after_cr);
			else
				pass();
			return 0;
		case SUBREAPER:
			setsid();

			pid = fork();
			if (pid < 0) {
				pr_perror("Failed to fork");
				return 1;
			} else if (pid == 0) {
				exit(worker(HELPER));
			}

			/* Reap the HELPER */
			waitpid(pid, &status, 0);
			if (!WIFEXITED(status) || WEXITSTATUS(status)) {
				pr_perror("Wrong exit status for helper: %d", status);
				return 1;
			}

			ret = prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
			if (ret) {
				pr_perror("Can't set child subreaper attribute, err = %d", ret);
				return 1;
			}

			/* Give controll to ORPHAN to save it's parent */
			futex_set_and_wake(fstate, TEST_SAVE);
			futex_wait_until(fstate, TEST_EXIT);
			return 0;
		case HELPER:
			pid = fork();
			if (pid < 0) {
				pr_perror("Failed to fork");
				return 1;
			} else if (pid == 0) {
				exit(worker(ORPHAN));
			}
			return 0;
		case ORPHAN:
			/*
			 * Wait until reparented to the pidns init. (By waiting
			 * for the SUBREAPER to reap our parent.)
			 */
			futex_wait_until(fstate, TEST_SAVE);

			*parent_before_cr = getppid();

			/* Return the controll back to MAIN worker to do C/R */
			futex_set_and_wake(fstate, TEST_CRIU);
			futex_wait_until(fstate, TEST_CHECK);

			*parent_after_cr = getppid();

			futex_set_and_wake(fstate, TEST_EXIT);
			return 0;
	}

	return 0;
}

int main(int argc, char **argv)
{
	void *ptr;

	BUG_ON(sizeof(*fstate) + sizeof(*parent_before_cr) + sizeof(*parent_after_cr) > 4096);

	ptr = mmap(NULL, 4096, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		pr_perror("Failed to alloc shared region");
		exit(1);
	}

	fstate = ptr;
	futex_set(fstate, TEST_FORK);
	parent_before_cr = ptr + sizeof(*fstate);
	parent_after_cr = ptr + sizeof(*fstate) + sizeof(*parent_before_cr);

	test_init(argc, argv);

	exit(worker(MAIN));
}
