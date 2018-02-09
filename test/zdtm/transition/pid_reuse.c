#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "zdtmtst.h"

const char *test_doc = "Tests that forking tasks with same pid does not break iterative dump";
const char *test_author = "Pavel Tikhomirov <ptikhomirov@virtuozzo.com>";

enum {
	VALUE_A = 1,
	VALUE_B = 2,
};

#define CHILD_NS_PID 11235

static int set_ns_next_pid(pid_t pid)
{
	char buf[32];
	int len, fd;

	fd = open("/proc/sys/kernel/ns_last_pid", O_WRONLY);
	if (fd < 0) {
		pr_perror("Failed to open ns_last_pid");
		return -1;
	}

	len = snprintf(buf, sizeof(buf), "%d", pid - 1);
	len -= write(fd, buf, len);
	if (len)
		pr_perror("Can't set ns_last_pid");
	close(fd);

	return len ? -1 : 0;
}

int main(int argc, char **argv)
{
	int pid, wpid, status;
	bool overwrite = true;
	bool wait = true;
	int *variable;
	void *mem;

	test_init(argc, argv);

	mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED) {
		pr_perror("Can't mmap memory region");
		return 1;
	}

	variable = (int *)mem;
	*variable = VALUE_A;

	test_daemon();

	while (wait) {
		if (set_ns_next_pid(CHILD_NS_PID))
			return 1;

		pid = fork();
		if (pid == -1) {
			pr_perror("fork");
			return 1;
		} else if (pid == 0) {
			if (overwrite)
				*variable = VALUE_B;
			test_waitsig();

			if (*variable != (overwrite ? VALUE_B : VALUE_A)) {
				pr_err("Wrong value in a variable after restore\n");
				exit(1);
			}
			exit(0);
		}

		if (pid != CHILD_NS_PID) {
			pr_err("Child started with wrong pid %d (expected %d)\n", pid, CHILD_NS_PID);
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			return 1;
		}

		/* Wait for next predump/dump finish */
		if (test_waitpre())
			wait = false;

		if (kill(pid, SIGTERM)) {
			pr_perror("kill");
			return 1;
		}

		wpid = waitpid(pid, &status, 0);
		if (wpid <= 0) {
			pr_perror("waitpid");
			return 1;
		}

		if (!WIFEXITED(status)) {
			fail("Task %d didn't exit", wpid);
			return 1;
		}

		if (WEXITSTATUS(status) != 0) {
			fail("Task %d exited with wrong code %d", wpid, WEXITSTATUS(status));
			return 1;
		}

		overwrite = false;
	}
	pass();
	return 0;
}