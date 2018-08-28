#include <sys/mount.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "zdtmtst.h"

const char *test_doc	= "Check ghost file on readonly fs mount restores fine";
const char *test_author	= "Pavel Tikhomirov <ptikhomirov@virtuozzo.com>";

#define GHOST_DATA "TeSt"

char *dirname;
TEST_OPTION(dirname, string, "directory name", 1);

int main(int argc, char **argv)
{
	char rw_mount[PATH_MAX], ro_mount[PATH_MAX];
	char file[PATH_MAX], ghost_file[PATH_MAX];
	char buf[PATH_MAX];
	int fd;

	test_init(argc, argv);

	if (mkdir(dirname, 0700)) {
		pr_perror("mkdir");
		return 1;
	}

	if (mount("zdtm_fs", dirname, "tmpfs", 0, NULL)) {
		pr_perror("mount");
		return 1;
	}

	if (mount(NULL, dirname, NULL, MS_PRIVATE, NULL)) {
		pr_perror("mount");
		return 1;
	}

	ssprintf(rw_mount, "%s/rw_mount", dirname);
	if (mkdir(rw_mount, 0700)) {
		pr_perror("mkdir");
		return 1;
	}

	if (mount("rw_mount", rw_mount, "tmpfs", 0, NULL)) {
		pr_perror("mount");
		return 1;
	}

	ssprintf(ro_mount, "%s/ro_mount", dirname);
	if (mkdir(ro_mount, 0700)) {
		pr_perror("mkdir");
		return 1;
	}

	if (mount(rw_mount, ro_mount, NULL, MS_BIND, NULL)) {
		pr_perror("mount");
		return 1;
	}

	if (mount(NULL, ro_mount, NULL, MS_RDONLY|MS_REMOUNT|MS_BIND, NULL)) {
		pr_perror("mount");
		return 1;
	}

	ssprintf(file, "%s/file", rw_mount);
	fd = open(file, O_CREAT|O_WRONLY, 0600);
	if (fd < 0) {
		pr_perror("open");
		return 1;
	}

	if (write(fd, GHOST_DATA, sizeof(GHOST_DATA)) != sizeof(GHOST_DATA)) {
		pr_perror("write");
		return 1;
	}

	close(fd);

	ssprintf(ghost_file, "%s/file", ro_mount);
	fd = open(ghost_file, O_RDONLY);
	if (fd < 0) {
		pr_perror("open");
		return 1;
	}

	if (unlink(file)) {
		pr_perror("unlink %s", file);
		return 1;
	}

	if (mount(NULL, rw_mount, NULL, MS_RDONLY|MS_REMOUNT|MS_BIND, NULL)) {
		pr_perror("mount");
		return 1;
	}

	test_daemon();
	test_waitsig();

	if (read(fd, buf, sizeof(GHOST_DATA)) != sizeof(GHOST_DATA)) {
		fail("Can't read from ghost file");
		return 1;
	}

	if (strcmp(buf, GHOST_DATA)) {
		fail("Wrong data in a ghost file");
		return 1;
	}

	close(fd);

	if (umount(ro_mount)) {
		pr_perror("Unable to umount %s", ro_mount);
		return 1;
	}

	if (umount(rw_mount)) {
		pr_perror("Unable to umount %s", rw_mount);
		return 1;
	}

	if (umount(dirname)) {
		pr_perror("Unable to umount %s", dirname);
		return 1;
	}

	pass();

	return 0;
}