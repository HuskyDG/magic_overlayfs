#include "logging.hpp"
#include "base.hpp"
#include "utils.hpp"

char *random_strc(int n){
    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd == -1) return nullptr;
    char *str = (char*)malloc(sizeof(char)*(n+1));
    if (str == nullptr) {
        close(urandom_fd);
        return nullptr;
    }
    for (int i=0;i<n;i++){
        char x = 0;
        read(urandom_fd, &x, 1);
        str[i] = 'a' + (x % ('z'-'a'+1));
    }
    str[n] = '\0';
    close(urandom_fd);
    return str;
}


bool starts_with(const char *s, const char *ss) {
    const char *str = strstr(s,ss);
    return str != nullptr && str == s;
}

bool fexist(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}
    

bool is_dir(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0 &&
           S_ISDIR(st.st_mode);
}

bool is_lnk(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0 &&
           S_ISLNK(st.st_mode);
}

bool mkdir_ensure(const char *path, int mode) {
    mkdir(path, mode);
    return is_dir(path);
}

int mkdirs(const char *path, int mode) {
    char s[strlen(path) + 1];
    s[0] = path[0];
    int a = 1, b = 1;
    while (a <= strlen(path)) {
        if (path[a] != '/' || path[a - 1] != '/') {
            s[b] = path[a];
            b++;
        }
        a++;
    }
    char *ss = s;
    while (ss[0] == '/')
        ss++;
    if (ss[0] == '\0')
        return 0;
    while ((ss = strchr(ss, '/')) != nullptr) {
        ss[0] = '\0';
        mkdir(s, mode);
        ss[0] = '/';
        ss++;
    }
    int ret = mkdir(s, mode);
    return ret;
}

char *dirname2(const char *path) {
    char s[strlen(path) + 1];
    char p[strlen(path) + 1];
    s[0] = path[0];
    int a = 1, b = 1;
    while (a <= strlen(path)) {
        if (path[a] != '/' || path[a - 1] != '/') {
            s[b] = path[a];
            b++;
        }
        a++;
    }
    char *ss = s;
    while (ss[0] == '/')
        ss++;
    if (ss[0] == '\0')
        return nullptr;
    while ((ss = strchr(ss, '/')) != nullptr) {
        ss[0] = '\0';
        strcpy(p, s);
        ss[0] = '/';
        ss++;
    }
    return strdup(p);
}

int getmod(const char *file) {
    int mode = 0;
    struct stat st;
    if (stat(file, &st))
        return -1;
    return st.st_mode & 0777;
}

int getuidof(const char *file) {
    struct stat st;
    if (stat(file, &st))
        return -1;
    return st.st_uid;
}

int getgidof(const char *file) {
    struct stat st;
    if (stat(file, &st))
        return -1;
    return st.st_gid;
}

int dump_file(const char *src, const char *dest) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
        return -1;
    int dest_fd = open(dest, O_RDWR | O_CREAT, 644);
    if (dest_fd < 0) {
        close(src_fd);
        return -1;
    }
    int buf[1024];
    while (read(src_fd, buf, sizeof(buf)))
        write(dest_fd, buf, sizeof(buf));
    close(src_fd);
    close(dest_fd);
    return 0;
}


int verbose_mount(const char *a, const char *b, const char *c, int d, const char *e) {
    int ret = mount(a,b,c,d,e);
    std::string mount_opts = "";
    if (d & MS_PRIVATE) mount_opts += ",private";
    else if (d & MS_SLAVE) mount_opts += ",slave";
    else if (d & MS_SHARED) mount_opts += ",shared";
    else if (d & MS_UNBINDABLE) mount_opts += ",unbindable";
    else {
        if (d & MS_RDONLY) mount_opts += ",ro"; else if ((d & MS_BIND) == 0) mount_opts += ",rw";
        if (d & MS_LAZYTIME) mount_opts += ",lazytime";
        if (d & MS_NODEV) mount_opts += ",nodev";
        if (d & MS_NOEXEC) mount_opts += ",noexec";
        if (d & MS_NOSUID) mount_opts += ",nosuid";
        if (d & MS_SYNCHRONOUS) mount_opts += ",sync";
        if (d & MS_NOATIME) mount_opts += ",noatime";
        if (d & MS_NODIRATIME) mount_opts += ",nodiratime";
        if (d & MS_RELATIME) mount_opts += ",relatime";
        if (d & MS_STRICTATIME) mount_opts += ",strictatime";
        if (d & MS_NOSYMFOLLOW) mount_opts += ",nosymfollow";
        if (d & MS_MANDLOCK) mount_opts += ",mand";
        if (d & MS_SILENT) mount_opts += ",silent";
        if (d & MS_REMOUNT) mount_opts += ",remount";
        if (d & MS_BIND) mount_opts += ",bind";
    }
    if (d & MS_REC) mount_opts += ",rec";
    if (e) {
        mount_opts += std::string(",") + e;
    }
    if (ret == 0) {
        LOGD("mount: %s%s%s%s\n", b, (a != nullptr && a[0] != '\0')? std::string(std::string(" <- ") + a).data() : "",
            c? std::string(std::string(" (") + c + ")").data() : "", (!str_empty(mount_opts.data()))? std::string(std::string(" [") + (mount_opts.data() + 1) + "]").data() : "");
    } else {
        PLOGE("mount: %s%s", (a != nullptr && a[0] != '\0')? std::string(std::string(a) + " -> ").data() : "", b);
    }
    return ret;
}

int verbose_umount(const char *a, int b) {
    LOGD("umount: %s\n", a);
    return umount2(a,b);
}

bool str_empty(const char *str) {
    return str == nullptr || str[0] == '\0';
}

const char *xgetenv(const char *name) {
    const char *val = getenv(name);
    LOGD("getenv: [%s]=[%s]\n", name? name : "", val? val : "");
    return val;
}

std::vector<std::string> split_ro(const std::string& str, const char delimiter) {
    std::vector<std::string> result;
    std::string current = "";

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == delimiter) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += str[i];
        }
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

int getenforce() {
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0) return -1;
    char val = -1;
    read(fd, &val, sizeof(val));
    close(fd);
    return (val == '0' || val == '1')? val - '0' : -1;
}


int setenforce(bool mode) {
    int fd = open("/sys/fs/selinux/enforce", O_RDWR);
    if (fd < 0) return -1;
    char val = '0' + mode;
    int ret = write(fd, &val, sizeof(val));
    close(fd);
    return ret;
}

int getfilecon(const char *path, char **con) {
    *con = (char*)malloc(sizeof(char)*256);
    return getxattr(path, "security.selinux", *con, sizeof(char)*255);
}

int setfilecon(const char *path, const char *con) {
    return setxattr(path, "security.selinux", con, (strlen(con) + 1)*sizeof(char), 0);
}

void freecon(char *con) {
    free(con);
}

