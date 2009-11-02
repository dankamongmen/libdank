#ifndef TEST_CUNNY
#define TEST_CUNNY

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/objects/logctx.h>

typedef int (*testptr)(void);

typedef struct declared_test {
	int disabled;
	testptr testfxn;
	const char *name;
	unsigned expected_result;// testresult enum
	unsigned mb_required;	 // 0 for "inconsequential" minimum limit (4MB)
	unsigned sec_required;	 // 0 for "inconsequential" minimum limit (10s)
} declared_test;

extern const declared_test CUNIT_TESTS[];
extern const declared_test ARCH_TESTS[];
extern const declared_test OFFT_TESTS[];
extern const declared_test MAGIC_TESTS[];
extern const declared_test FDS_TESTS[];
extern const declared_test EVENT_TESTS[];
extern const declared_test VM_TESTS[];
extern const declared_test MMAP_TESTS[];
extern const declared_test PTHREADS_TESTS[];
extern const declared_test USTRING_TESTS[];
extern const declared_test SLALLOC_TESTS[];
extern const declared_test STRING_TESTS[];
extern const declared_test LOGCTX_TESTS[];
extern const declared_test TIMEVAL_TESTS[];
extern const declared_test XML_TESTS[];
extern const declared_test FILECONF_TESTS[];
extern const declared_test PROCFS_TESTS[];
extern const declared_test CRLFREADER_TESTS[];
extern const declared_test CTLSERVER_TESTS[];
extern const declared_test HEALTH_TESTS[];
extern const declared_test RFC2396_TESTS[];
extern const declared_test RFC3330_TESTS[];
extern const declared_test LRUPAT_TESTS[];
extern const declared_test NETLINK_TESTS[];
extern const declared_test HEX_TESTS[];
extern const declared_test DLSYM_TESTS[];
extern const declared_test INTERVAL_TREE_TESTS[];

int ctlclient_quiet(const char *cmd);
pid_t ctlclient_quiet_nowait(const char *cmd);
#define CUNIT_CTLSERVER "cunit-ctlserver"

#define EXIT_TESTSUCCESS	0x01
#define EXIT_TESTFAILED		0x02
#define EXIT_MEMLEAK		0x04
#define EXIT_NEEDMORERAM	0x08
#define EXIT_MCHECKHACK		0x10 // mcheck returns 127, no good for &|
#define EXIT_INTERNALEXIT	0x20 // internal exit could be anything
// a unit test calling _exit will avoid exit handlers we've put in its
// place to catch such chicanery. we want to distinguish the success case
// from this if we can. the failure case obviously is not so bad. we don't
// use 0x7f, because that of course is equivalent to -1 in 7 bits' worth
// of two's-complement. we don't use 0x7e because of one's complement :D.
// furthermore, we don't want intersection with TESTFAILED or MEMLEAK,
// so drop one more to 0x7c -nlb
#define EXIT_HACKTESTSUCCESS 	0x7c
#define EXIT_SIGNAL		0x80
#define EXIT_MCHECK       	0xff

#ifdef __cplusplus
}
#endif

#endif
