#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>
#include <syscall.h>
#include <sys/mount.h>

#include "zdtmtst.h"

const char *test_doc = "Check if root mount remains read-only after c/r";
const char *test_author = "Pavel Tikhomirov <ptikhomirov@virtuozzo.com>";

char *dirname;
TEST_OPTION(dirname, string, "directory name", 1);

int root_mount_readonly(bool *readonly)
{
	struct mnt_id_req req = {
		.size = sizeof(req),
		.param = STATMOUNT_MNT_BASIC,
	};
	struct statmount stm;
	struct statx stx;
	int fd;

	fd = open("/", O_DIRECTORY);
	if (fd < 0) {
		pr_perror("open");
		return 1;
	}

	if (statx(fd, "", AT_EMPTY_PATH, STATX_MNT_ID_UNIQUE, &stx)) {
		pr_perror("statx");
		close(fd);
		return 1;
	}
	close(fd);

	req.mnt_id = stx.stx_mnt_id;
	if (syscall(SYS_statmount, &req, &stm, sizeof(stm), 0)) {
		pr_perror("statmount");
		return 1;
	}

	*readonly = stm.mnt_attr & MOUNT_ATTR_RDONLY;
	return 0;
}

int main(int argc, char **argv)
{
	bool readonly;

	test_init(argc, argv);

	if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY | MS_BIND, NULL)) {
		pr_perror("mount");
		return 1;
	}

	test_daemon();
	test_waitsig();

	if (root_mount_readonly(&readonly))
		return 1;

	if (!readonly) {
		fail("Root mount become writable after c/r");
		return 1;
	}

	pass();
	return 0;
}
