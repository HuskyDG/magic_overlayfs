#include "base.hpp"

char *random_strc(int n);
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
std::vector<std::string> split_ro(const std::string& str, const char delimiter);
int getenforce();
int setenforce(bool mode);
int getfilecon(const char *path, char **con);
int setfilecon(const char *path, const char *con);
void freecon(char *con);

template <typename T>
class reversed_container {
public:
    reversed_container(T &base) : base(base) {}
    decltype(std::declval<T>().rbegin()) begin() { return base.rbegin(); }
    decltype(std::declval<T>().crbegin()) begin() const { return base.crbegin(); }
    decltype(std::declval<T>().crbegin()) cbegin() const { return base.crbegin(); }
    decltype(std::declval<T>().rend()) end() { return base.rend(); }
    decltype(std::declval<T>().crend()) end() const { return base.crend(); }
    decltype(std::declval<T>().crend()) cend() const { return base.crend(); }
private:
    T &base;
};

template <typename T>
reversed_container<T> reversed(T &base) {
    return reversed_container<T>(base);
}


