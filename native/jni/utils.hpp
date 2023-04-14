#include "base.hpp"

std::string random_strc(int n);
bool starts_with(const char *s, const char *ss);
bool is_dir(const char *path);
bool is_lnk(const char *path);
bool fexist(const char *path);
bool mkdir_ensure(const char *path, int mode);
int mkdirs(const char *path, int mode);
char *dirname2(const char *path);
int getmod(const char *file);
int getuidof(const char *file);
int getgidof(const char *file);
int dump_file(const char *src, const char *dest);
int verbose_mount(const char *a, const char *b, const char *c, int d, const char *e);
int verbose_umount(const char *a, int b);
const char *xgetenv(const char *name);
bool str_empty(const char *str);

