#ifndef LIBDANK_UTILS_FS
#define LIBDANK_UTILS_FS

#ifdef __cplusplus
extern "C" {
#endif

struct statfs;

int fs_memorybacked(const struct statfs *);
int fs_largepagebacked(const struct statfs *);
int mount_tmpfs(const char *);

#ifdef __cplusplus
}
#endif

#endif
