#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "zdtmtst.h"

const char *test_doc	= "Check tmpfs mount";
const char *test_author	= "Pavel Emelianov <xemul@parallels.com>";

char *dirname;
TEST_OPTION(dirname, string, "directory name", 1);

int main(int argc, char **argv)
{
	char testfile[PATH_MAX];
	int fd;

	test_init(argc, argv);

	mkdir(dirname, 0700);
	if (mount("none", dirname, "tmpfs", 0, "") < 0) {
		fail("Can't mount tmpfs");
		return 1;
	}

	snprintf(testfile, PATH_MAX, "%s/testfile", dirname);
	fd = open(testfile, O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		fail("Can't create file");
		return 1;
	}
	close(fd);

	if (mount("none", dirname, "tmpfs", 0, "") < 0) {
		fail("Can't mount tmpfs");
		return 1;
	}

	test_daemon();
	test_waitsig();

	if (umount(dirname)) {
		fail("Can't umount");
		return 1;
	}

	if (access(testfile, F_OK) < 0) {
		fail("Can't access testfile");
		return 1;
	}

	pass();
	return 0;
}
