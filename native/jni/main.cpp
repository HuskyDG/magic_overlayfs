#include "base.hpp"
#include "logging.hpp"
#include "mountinfo.hpp"
#include "utils.hpp"

using namespace std;

#define xmount(a,b,c,d,e) verbose_mount(a,b,c,d,e)
#define umount2(a,b) verbose_umount(a,b)

#define UNDER(s) (starts_with(info.target.data(), s "/") || info.target == s)

#define MAKEDIR(s) \
    if (std::find(mountpoint.begin(), mountpoint.end(), "/" s) != mountpoint.end()) { \
        mkdir(std::string(tmp_dir + "/" s).data(), 0755); \
        if ((dirfp = opendir("/" s)) != nullptr) { \
            char buf[4098]; \
            struct stat st; \
            while ((dp = readdir(dirfp)) != nullptr) { \
                snprintf(buf, sizeof(buf) - 1, "/" s "/%s", dp->d_name); \
                if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || \
                lstat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) \
                    continue; \
                mkdir(std::string(tmp_dir + buf).data(), 0755); \
                mount_list.push_back(buf); \
            } \
            closedir(dirfp); \
        } \
    }

#define CLEANUP \
    LOGI("clean up\n"); \
    umount2(tmp_dir.data(), MNT_DETACH); \
    rmdir(tmp_dir.data());

int log_fd = -1;
std::string tmp_dir;
std::vector<string> mountpoint;
std::vector<mount_info> mountinfo;

static void collect_mounts() {
    // sort mountinfo, skip unnecessary mounts
    mountpoint.clear();
    mountinfo.clear();
    do {
        auto current_mount_info = parse_mount_info("self");
        std::reverse(current_mount_info.begin(), current_mount_info.end());
        for (auto &info : current_mount_info) {
            if (UNDER("/system") ||
                 UNDER("/vendor") ||
                 UNDER("/system_ext") ||
                 UNDER("/product") ||
                 UNDER("/odm") ||
                 UNDER("/oem") ||
                 UNDER("/vendor_dlkm") ||
                 UNDER("/odm_dlkm") ||
                 UNDER("/my_custom") ||
                 UNDER("/my_engineering") ||
                 UNDER("/my_heytap") ||
                 UNDER("/my_manifest") ||
                 UNDER("/my_preload") ||
                 UNDER("/my_product") ||
                 UNDER("/my_region") ||
                 UNDER("/my_stock") ||
                 UNDER("/my_version") ||
                 UNDER("/my_company") ||
                 UNDER("/my_carrier") ||
                 UNDER("/my_region") ||
                 UNDER("/my_company") ||
                 UNDER("/my_bigball")) {
                for (auto &s : mountpoint) {
                    //   /a/b/c <--- under a (skip)
                    //   /a
                    //   /a/b
                    if (s == info.target || starts_with(string(info.target + "/").data(), s.data()))
                        goto next_mountpoint;
                }
                //printf("new mount: %s\n", info.target.data());
                mountpoint.emplace_back(info.target);
                mountinfo.emplace_back(info);
            }
            next_mountpoint:
            continue;
        }
    } while(false);
}

static int do_remount(int flags = 0, int exclude_flags = 0) {
    collect_mounts();
    struct statvfs stvfs{};
    exclude_flags |= MS_BIND;
    for (auto &info : mountpoint) {
        statvfs(info.data(), &stvfs);
        xmount(nullptr, info.data(), nullptr, MS_REMOUNT | (stvfs.f_flag & ~exclude_flags) | flags, nullptr);
    }
    return 0;
}

int main(int argc, const char **argv) {
    char *argv0 = strdup(argv[0]);
    const char *bname = basename(argv0);

    if ((strcmp(bname, "magic_remount_rw") == 0) || ((argc > 1) && (strcmp(argv[1], "--remount-rw") == 0))) {
        return do_remount(0, MS_RDONLY);
    } else if ((strcmp(bname, "magic_remount_ro") == 0) || ((argc > 1) && (strcmp(argv[1], "--remount-ro") == 0))) {
        return do_remount(MS_RDONLY);
    }

    bool overlay = false;
    FILE *fp = fopen("/proc/filesystems", "re");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strlen(buf)-1] = '\0';
            if (strcmp(buf + 6, "overlay") == 0) {
                overlay = true;
                break;
            }
        }
        fclose(fp);
    }
    if (!overlay) {
        printf("No overlay supported by kernel!\n");
        return 1;
    }
    if (argc<2) {
        printf("You forgot to tell me the write-able folder :v\n");
        return 1;
    }
    if (strcmp(argv[1], "--test") == 0) {
        argc--;
        argv++;
        if (argc >= 3 && strcmp(argv[1], "--check-ext4") == 0) {
            struct statfs stfs{};
            return (statfs(argv[2], &stfs) == 0 && stfs.f_type == EXT4_SUPER_MAGIC)?
                0 : 1;
        }
        return 0;
    }
    if (argv[1][0] != '/') {
        printf("Please tell me the full path of folder >:)\n");
        return 1;
    }

    struct stat z;
    if (stat(argv[1], &z)) {
        printf("%s does not exist!\n", argv[1]);
        return 1;
    }
    if (!S_ISDIR(z.st_mode)) {
        printf("This is not folder %s\n", argv[1]);
        return 1;
    }

    log_fd = open("/cache/overlayfs.log", O_RDWR | O_CREAT | O_APPEND, 0666);
    LOGI("* Mount OverlayFS started\n");
    collect_mounts();

    const char *OVERLAY_MODE_env = xgetenv("OVERLAY_MODE");
    const char *OVERLAYLIST_env = xgetenv("OVERLAYLIST");
    const char *MAGISKTMP_env = xgetenv("MAGISKTMP");

    if (OVERLAYLIST_env == nullptr) OVERLAYLIST_env = "";
    int OVERLAY_MODE = (OVERLAY_MODE_env)? atoi(OVERLAY_MODE_env) : 0;

    const char *mirrors = nullptr;
    if (!str_empty(MAGISKTMP_env)) {
        // use strdup as std::string, memory is automatically managed by the class and released when the string object goes out of scope
        mirrors = strdup(string(string(MAGISKTMP_env) + "/.magisk/mirror").data());
        if (stat(mirrors, &z) != 0 || !S_ISDIR(z.st_mode)) {
            free((void*)mirrors);    
            mirrors = nullptr;
        }
    }
    if (mirrors) {
        LOGD("Magisk mirrors path is %s\n", mirrors);
    }

    // list of directories should be mounted!
    std::vector<string> mount_list;

    tmp_dir = std::string("/mnt/") + "overlayfs_" + random_strc(20);
    if (mkdir(tmp_dir.data(), 750) != 0) {
        LOGE("Cannot create temp folder, please make sure /mnt is clean and write-able!\n");
        return -1;
    }
    mkdir(std::string(std::string(argv[1]) + "/upper").data(), 0750);
    mkdir(std::string(std::string(argv[1]) + "/worker").data(), 0750);
    mkdir(std::string(std::string(argv[1]) + "/master").data(), 0750);
    xmount("tmpfs", tmp_dir.data(), "tmpfs", 0, nullptr);

    struct mount_info system;
    system.target = "/system";
    system.type = "ext4";
    if (std::find(mountpoint.begin(), mountpoint.end(), "/system") == mountpoint.end()) {
        mountinfo.emplace_back(system);
        mountpoint.emplace_back("/system");
    }
    DIR *dirfp;
    struct dirent *dp;

    MAKEDIR("system")
    MAKEDIR("vendor")
    MAKEDIR("system_ext")
    MAKEDIR("product")
    MAKEDIR("odm")
    MAKEDIR("oem")
    MAKEDIR("vendor_dlkm")
    MAKEDIR("odm_dlkm")
    MAKEDIR("my_custom")
    MAKEDIR("my_engineering")
    MAKEDIR("my_heytap")
    MAKEDIR("my_manifest")
    MAKEDIR("my_preload")
    MAKEDIR("my_product")
    MAKEDIR("my_region")
    MAKEDIR("my_stock")
    MAKEDIR("my_version")
    MAKEDIR("my_company")
    MAKEDIR("my_carrier")
    MAKEDIR("my_region")
    MAKEDIR("my_company")
    MAKEDIR("my_bigball")

    mountpoint.clear();

    {
        std::string masterdir = std::string(argv[1]) + "/master";
        if (!str_empty(OVERLAYLIST_env)) {
            if (strchr(OVERLAYLIST_env, ':') != nullptr) {
                std::string opts = "lowerdir=";
                opts += OVERLAYLIST_env;
                xmount("overlay", masterdir.data(), "overlay", 0, opts.data());
            } else {
                xmount(OVERLAYLIST_env, masterdir.data(), nullptr, MS_BIND, nullptr);
            }
        }
    }

    LOGI("** Prepare mounts\n");
    // mount overlayfs for subdirectories of /system /vendor /product /system_ext
    std::reverse(mountinfo.begin(), mountinfo.end());
    for (auto &info : mount_list) {
        struct stat st;
        if (stat(info.data(), &st))
            continue;
        std::string tmp_mount = tmp_dir + info;

        std::string upperdir = std::string(argv[1]) + "/upper" + info;
        std::string workerdir = std::string(argv[1]) + "/worker" + info;
        std::string masterdir = std::string(argv[1]) + "/master" + info;
        char *con;
        {
            char *s = strdup(info.data());
            char *ss = s;
            while ((ss = strchr(ss, '/')) != nullptr) {
                ss[0] = '\0';
                auto sub = std::string(argv[1]) + "/upper" + s;
                if (mkdir(sub.data(), 0755) == 0 && getfilecon(s, &con) >= 0) {
                    LOGD("clone attr [%s] from [%s]\n", con, s);
                    chown(sub.data(), getuidof(s), getgidof(s));
                    chmod(sub.data(), getmod(s));
                    setfilecon(sub.data(), con);
                    freecon(con);
                }
                ss[0] = '/';
                ss++;
            }
            free(s);
        };
        
        {
            if (mkdir(upperdir.data(), 0755) == 0 && getfilecon(info.data(), &con) >= 0) {
                LOGD("clone attr [%s] from [%s]\n", con, info.data());
                chown(upperdir.data(), getuidof(info.data()), getgidof(info.data()));
                chmod(upperdir.data(), getmod(info.data()));
                setfilecon(upperdir.data(), con);
                freecon(con);
            }
            mkdirs(workerdir.data(), 0755);

            if (!is_dir(upperdir.data()) ||
                !is_dir(workerdir.data())) {
                LOGD("setup upperdir or workdir failed!\n");
                CLEANUP
                return 1;
            }
        }
        {
            bool module_node_is_dir = is_dir(masterdir.data());
            std::string opts;
            opts += "lowerdir=";
            if (module_node_is_dir)
                opts += masterdir + ":";
            opts += info.data();
            opts += ",upperdir=";
            opts += upperdir;
            opts += ",workdir=";
            opts += workerdir;
            
            // 0 - read-only
            // 1 - read-write default
            // 2 - read-only locked
            
            if (OVERLAY_MODE == 2 || xmount("overlay", tmp_mount.data(), "overlay", ((OVERLAY_MODE == 1)? 0 : MS_RDONLY), opts.data())) {
                opts = "lowerdir=";
                opts += upperdir;
                opts += ":";
                if (module_node_is_dir)
                    opts += masterdir + ":";
                opts += info.data();
                if (xmount("overlay", tmp_mount.data(), "overlay", 0, opts.data())) {
                    LOGW("Unable to add [%s], ignore!\n", info.data());
                    continue;
                }
            }
        }
        mountpoint.emplace_back(info);
    }

    // restore stock mounts if possible
    // if stock mount is directory, merge it with overlayfs
    // if stock mount is file, then we bind mount it back
    for (auto &mnt : mountinfo) {
        auto info = mnt.target;
        std::string tmp_mount = tmp_dir + info;
        std::string upperdir = std::string(argv[1]) + "/upper" + info;
        std::string workerdir = std::string(argv[1]) + "/worker" + info;
        std::string masterdir = std::string(argv[1]) + "/master" + info;
        bool module_node_is_dir = is_dir(masterdir.data());
        bool module_node_exist = fexist(masterdir.data());
        bool upper_node_is_dir = is_dir(upperdir.data());
        bool upper_node_exist = fexist(upperdir.data());
        struct stat st;
        if (lstat(info.data(), &st) != 0 || // target does not exist, it could be deleted by modules
            (((upper_node_exist && !upper_node_is_dir) ||
              (!upper_node_exist && module_node_exist && !module_node_is_dir)) && S_ISDIR(st.st_mode))) // module add file but original is folder
            continue;
        for (auto &s : mount_list) {
            // only care about mountpoint under overlayfs mounted subdirectories
            if (!starts_with(info.data(), string(s + "/").data()))
               continue;
            char *con;
            if (!S_ISDIR(st.st_mode)) {
                // skip bind mount if there is modification for this file
                if (access(masterdir.data(), F_OK) == 0 ||
                    access(upperdir.data(), F_OK) == 0)
                    continue;
                goto bind_mount;
            }
            {
                char *s = strdup(info.data());
                char *ss = s;
                while ((ss = strchr(ss, '/')) != nullptr) {
                    ss[0] = '\0';
                    auto sub = std::string(argv[1]) + "/upper" + s;
                    if (mkdir(sub.data(), 0755) == 0 && getfilecon(s, &con) >= 0) {
                        LOGD("clone attr [%s] from [%s]\n", con, s);
                        chown(sub.data(), getuidof(s), getgidof(s));
                        chmod(sub.data(), getmod(s));
                        setfilecon(sub.data(), con);
                        freecon(con);
                    }
                    ss[0] = '/';
                    ss++;
                }
                free(s);
            };
            {
                if (mkdir(upperdir.data(), 0755) == 0 && getfilecon(info.data(), &con) >= 0) {
                    LOGD("clone attr [%s] from [%s]\n", con, info.data());
                    chown(upperdir.data(), getuidof(info.data()), getgidof(info.data()));
                    chmod(upperdir.data(), getmod(info.data()));
                    setfilecon(upperdir.data(), con);
                    freecon(con);
                }
                mkdirs(workerdir.data(), 0755);
    
                if (!is_dir(upperdir.data()) ||
                    !is_dir(workerdir.data())) {
                    LOGD("setup upperdir or workdir failed!\n");
                    CLEANUP
                    return 1;
                }
            }
            {
                std::string opts;
                opts += "lowerdir=";
                if (stat(masterdir.data(), &st) == 0 && S_ISDIR(st.st_mode))
                    opts += masterdir + ":";
                opts += info.data();
                opts += ",upperdir=";
                opts += upperdir;
                opts += ",workdir=";
                opts += workerdir;
                
                // 0 - read-only
                // 1 - read-write default
                // 2 - read-only locked
                
                if (OVERLAY_MODE == 2 || xmount("overlay", tmp_mount.data(), "overlay", ((OVERLAY_MODE == 1)? 0 : MS_RDONLY), opts.data())) {
                    opts = "lowerdir=";
                    opts += upperdir;
                    opts += ":";
                    if (stat(masterdir.data(), &st) == 0 && S_ISDIR(st.st_mode))
                        opts += masterdir + ":";
                    opts += info.data();
                    if (xmount("overlay", tmp_mount.data(), "overlay", 0, opts.data())) {
                        // for some reason, overlayfs does not support some filesystems such as vfat, tmpfs, f2fs
                        // then bind mount it back but we will not be able to modify its content
                        LOGW("mount overlayfs failed, fall to bind mount!\n");
                        goto bind_mount;
                    }
                }
            }
            goto mount_done;
               
            bind_mount:
            if (xmount(info.data(), tmp_mount.data(), nullptr, MS_BIND, nullptr)) {
                // mount fails
                LOGE("mount failed, abort!\n");
                CLEANUP
                return 1;
            }
            
            mount_done:
            mountpoint.emplace_back(info);
            break;
        }
    }


    LOGI("** Loading overlayfs\n");
    std::vector<string> mounted;
    for (auto &info : mountpoint) {
        std::string tmp_mount = tmp_dir + info;
        if (xmount(tmp_mount.data(), info.data(), nullptr, MS_BIND, nullptr) ||
            mount("", info.data(), nullptr, MS_PRIVATE, nullptr) ||
            mount("", info.data(), nullptr, MS_SHARED, nullptr)) {
            LOGE("mount failed, abort!\n");
            // revert all mounts
            std::reverse(mounted.begin(), mounted.end());
            for (auto &dir : mounted) {
                umount2(dir.data(), MNT_DETACH);
            }
            CLEANUP
            return 1;
        }
        mounted.emplace_back(info);
    }
    // inject mount back to to magisk mirrors so Magic mount won't override it
    if (mirrors != nullptr) {
        LOGI("** Loading overlayfs mirrors\n");
        for (auto &info : mountpoint) {
            std::string mirror_dir = string(mirrors) + info;
            if (access(mirror_dir.data(), F_OK) == 0) {
                xmount(info.data(), mirror_dir.data(), nullptr, MS_BIND, nullptr);
                mount("", mirror_dir.data(), nullptr, MS_PRIVATE, nullptr);
                mount("", mirror_dir.data(), nullptr, MS_SHARED, nullptr);
            }
        }
    }
    LOGI("mount done!\n");
    CLEANUP
    return 0;
}
