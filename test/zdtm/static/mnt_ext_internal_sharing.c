#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "zdtmtst.h"
#include "lock.h"

const char *test_doc	= "Check internal sharing for external mount";
const char *test_author	= "Pavel Tikhomirov <ptikhomirov@virtuozzo.com>";

char *dirname = "mnt_ext_internal_sharing.test";
char *source = "zdtm_ext_internal_sharing";
#define SUBDIR "subdir"
TEST_OPTION(dirname, string, "directory name", 1);

enum {
	TEST_START,
	TEST_STARTED,
	TEST_EXIT,
	TEST_EXITED,
};

struct shared {
	futex_t fstate;
	int ret;
} *sh;

#define BUF_SIZE 4096

int pid_mntinfo_get_shid(char *pid)
{
	char path[PATH_MAX], line[BUF_SIZE];
	FILE *mountinfo;
	char *hyphen, *shared;
	int ret = -1;

	sprintf(path, "/proc/%s/mountinfo", pid);
	mountinfo = fopen(path, "r");
	if (!mountinfo) {
		pr_perror("fopen");
		return ret;
	}

	while (fgets(line, sizeof(line), mountinfo)) {
		hyphen = strchr(line, '-');
		if (!hyphen) {
			pr_perror("no hyphen in mountinfo");
			break;
		}

		if (!strstr(hyphen + 1, source))
			continue;

		shared = strstr(line, "shared:");
		if (!shared) {
			pr_err("no shared id\n");
			break;
		}

		ret = atoi(shared + 7);
		break;
	}

	fclose(mountinfo);
	return ret;
}

int secondary_mntns_child()
{
	if (unshare(CLONE_NEWNS)) {
		pr_perror("unshare");
		sh->ret = 1;
		futex_abort_and_wake(&sh->fstate);
		return 1;
	}
	futex_set_and_wake(&sh->fstate, TEST_STARTED);
	futex_wait_until(&sh->fstate, TEST_EXIT);
	/* These task is just holding the reference to secondary mntns */
	futex_set_and_wake(&sh->fstate, TEST_EXITED);
	return 0;
}

int main(int argc, char ** argv)
{
	char src[PATH_MAX], dst[PATH_MAX], nsdst[PATH_MAX];
	char spid[BUF_SIZE], *root;
	char *external_source = "/tmp/zdtm_ext_internal_sharing.XXXXXX";
	char *zdtm_newns = getenv("ZDTM_NEWNS");
	int pid, shid_self = -1, shid_pid = -1, status;

	root = getenv("ZDTM_ROOT");
	if (root == NULL) {
		pr_perror("root");
		return 1;
	}

	sprintf(dst, "%s/%s", root, dirname);

	if (!zdtm_newns) {
		pr_perror("ZDTM_NEWNS is not set");
		return 1;
	} else if (strcmp(zdtm_newns, "1")) {
		goto test;
	}

	/* Create private tmpfs mount in CRIU mntns - source for "external" */
	mkdir(external_source, 755);
	sprintf(src, "%s/%s", external_source, SUBDIR);
	if (mount(source, external_source, "tmpfs", 0, NULL)) {
		pr_perror("mount");
		return 1;
	}
	if (mount(NULL, external_source, NULL, MS_PRIVATE, NULL)) {
		pr_perror("bind");
		return 1;
	}
	mkdir(src, 755);

	/* Create "external" mount in testct fsroot in temporary mntns */
	if (unshare(CLONE_NEWNS)) {
		pr_perror("unshare");
		return 1;
	}
	mkdir(dst, 755);
	if (mount(src, dst, NULL, MS_BIND, NULL)) {
		pr_perror("bind");
		return 1;
	}

test:
	test_init(argc, argv);

	sh = mmap(NULL, sizeof(struct shared), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (sh == MAP_FAILED) {
		pr_perror("Failed to alloc shared region");
		exit(1);
	}

	futex_set(&sh->fstate, TEST_START);
	sh->ret = 0;

	sprintf(nsdst, "/%s", dirname);

	/* Make "external" mount to have internal sharing */
	if (mount(NULL, nsdst, NULL, MS_SHARED, NULL)) {
		pr_perror("bind");
		return 1;
	}

	/* Create secondary mntns and second "external" mount */
	pid = fork();
	if (pid < 0) {
		pr_perror("fork");
		return 1;
	} else if (pid == 0) {
		exit(secondary_mntns_child());
	}

	futex_wait_until(&sh->fstate, TEST_STARTED);
	if (sh->ret != 0) {
		pr_err("error in child\n");
		return 1;
	}

	test_daemon();
	test_waitsig();

	/*
	 * Check mounts in primary and secondary
	 * mntnses are shared to each other.
	 */
	sprintf(spid, "%d", pid);
	shid_pid = pid_mntinfo_get_shid(spid);
	shid_self = pid_mntinfo_get_shid("self");

	/* Cleanup */
	futex_set_and_wake(&sh->fstate, TEST_EXIT);
	futex_wait_until(&sh->fstate, TEST_EXITED);

	while (wait(&status) > 0) {
		if (!WIFEXITED(status) || WEXITSTATUS(status)) {
			fail("Wrong exit status: %d", status);
			return 1;
		}
	}

	if (shid_pid == -1 || shid_self == -1 || shid_pid != shid_self) {
		fail("Shared ids does not match");
		return 1;
	}

	pass();

	return 0;
}
