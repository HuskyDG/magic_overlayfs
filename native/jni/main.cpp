#include "base.hpp"
#include "logging.hpp"
#include "mountinfo.hpp"
#include "utils.hpp"

using namespace std;

#define xmount(a,b,c,d,e) verbose_mount(a,b,c,d,e)
#define umount2(a,b) verbose_umount(a,b)

#define UNDER(s) (starts_with(info.target.data(), s "/") || info.target == s)

#define MAKEDIR(s) \
    if (lstat("/" s, &st_part) == 0 && S_ISDIR(st_part.st_mode)) { \
        mkdir(std::string(overlay_tmpdir + "/" s).data(), 0755); \
        mount_list.push_back("/" s); \
    }

#define CLEANUP \
    LOGI("clean up\n"); \
    umount2(overlay_tmpdir.data(), MNT_DETACH); \
    rmdir(overlay_tmpdir.data());

int log_fd = -1;
std::string overlay_tmpdir;
std::vector<mount_info> mountinfo;

static void collect_mounts() {
    // sort mountinfo, skip unnecessary mounts
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
                 UNDER("/my_bigball") ||
                 UNDER("/prism") ||
                 UNDER("/optics")) {
                for (auto &s : mountinfo) {
                    //   /a/b/c <--- under a (skip)
                    //   /a
                    //   /a/b
                    if (s.target == info.target || starts_with(info.target.data(), string(s.target + "/").data()))
                        goto next_mountpoint;
                }
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
    for (auto &mnt : mountinfo) {
        auto info = mnt.target;
        statvfs(info.data(), &stvfs);
        if (mnt.type == "overlay" || mnt.type == "tmpfs") {
            LOGD("%s [%s] (%s)\n", (mount(nullptr, info.data(), nullptr, MS_REMOUNT | (stvfs.f_flag & ~exclude_flags) | flags, nullptr) == 0)?
                 "remounted" : "remount failed", info.data(), mnt.type.data());
        } else {
            LOGD("%s [%s] (%s)\n", "skipped", info.data(), mnt.type.data());
        }
    }
    return 0;
}

static int unmount_ksu_overlay() {
    collect_mounts();
    std::reverse(mountinfo.begin(), mountinfo.end());
    for (auto &mnt : mountinfo) {
        if (mnt.source == "KSU") {
            umount2(mnt.target.data(), MNT_DETACH);
        }
    }
    return 0;
}

static std::string get_lowerdirs(std::vector<std::string> list, const char *sub) {
    std::string lowerdir = "";
    for (auto it = list.begin(); it != list.end(); it++) {
           auto dir = *it + sub;
           if (is_dir(dir.data()))
               lowerdir+= dir + ":";
    }
    return lowerdir;
}

int main(int argc, const char **argv) {
    char *argv0 = strdup(argv[0]);
    const char *bname = basename(argv0);

    if ((strcmp(bname, "magic_remount_rw") == 0) || ((argc > 1) && (strcmp(argv[1], "--remount-rw") == 0))) {
        return do_remount(0, MS_RDONLY);
    } else if ((strcmp(bname, "magic_remount_ro") == 0) || ((argc > 1) && (strcmp(argv[1], "--remount-ro") == 0))) {
        return do_remount(MS_RDONLY);
    } else if ((argc > 1) && (strcmp(argv[1], "--unmount-ksu") == 0)) {
        return unmount_ksu_overlay();
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

    overlay_tmpdir = std::string("/mnt/") + "overlayfs_" + random_strc(20);
    if (mkdirs(overlay_tmpdir.data(), 750) != 0) {
        LOGE("Cannot create temp folder, please make sure /mnt is clean and write-able!\n");
        return -1;
    }
    mkdir(std::string(std::string(argv[1]) + "/upper").data(), 0750);
    mkdir(std::string(std::string(argv[1]) + "/worker").data(), 0750);

    xmount("tmpfs", overlay_tmpdir.data(), "tmpfs", 0, nullptr);
    mkdir(std::string(overlay_tmpdir + "/master").data(), 0750);

    {
        struct stat st_part;
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
        MAKEDIR("prism")
        MAKEDIR("optics")
    }

    {
        std::string masterdir = overlay_tmpdir + "/master";
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
    auto module_list = split_ro(OVERLAYLIST_env, ':');

    LOGI("** Prepare mounts\n");
    // mount overlayfs for subdirectories of /system /vendor /product /system_ext
    std::reverse(mountinfo.begin(), mountinfo.end());
    for (auto &info : mount_list) {
        struct stat st;
        if (stat(info.data(), &st))
            continue;
        std::string tmp_mount = overlay_tmpdir + info;

        std::string upperdir = std::string(argv[1]) + "/upper" + info;
        std::string workerdir = std::string(argv[1]) + "/worker/inode:" + std::to_string(st.st_ino);
        std::string masterdir = overlay_tmpdir + "/master" + info;
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
            std::string opts;
            opts += "lowerdir=";
            opts += get_lowerdirs(module_list, info.data());
            opts += info.data();
            opts += ",upperdir=";
            opts += upperdir;
            opts += ",workdir=";
            opts += workerdir;
            
            // 0 - read-only
            // 1 - read-write default
            // 2 - read-only locked
            
            if (OVERLAY_MODE == 2 || xmount("overlay", tmp_mount.data(), "overlay", MS_RDONLY | MS_NOATIME, opts.data())) {
                opts = "lowerdir=";
                opts += upperdir;
                opts += ":";
                opts += get_lowerdirs(module_list, info.data());
                opts += info.data();
                if (xmount("overlay", tmp_mount.data(), "overlay", MS_RDONLY | MS_NOATIME, opts.data())) {
                    LOGW("Unable to add [%s], ignore!\n", info.data());
                    continue;
                }
            }
        }
    }

    // restore stock mounts if possible
    // if stock mount is directory, merge it with overlayfs
    // if stock mount is file, then we bind mount it back
    for (auto &mnt : mountinfo) {
        auto info = mnt.target;
        struct stat st;
        if (stat(info.data(), &st))
            continue;
        std::string tmp_mount = overlay_tmpdir + info;
        std::string upperdir = std::string(argv[1]) + "/upper" + info;
        std::string workerdir = std::string(argv[1]) + "/worker/inode:" + std::to_string(st.st_ino);
        std::string masterdir = overlay_tmpdir + "/master" + info;
        bool module_node_is_dir = is_dir(masterdir.data());
        bool module_node_exist = fexist(masterdir.data());
        bool upper_node_is_dir = is_dir(upperdir.data());
        bool upper_node_exist = fexist(upperdir.data());
        if (lstat(tmp_mount.data(), &st) != 0 || // target does not exist, it could be deleted by modules
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
                opts += get_lowerdirs(module_list, info.data());
                opts += info.data();
                opts += ",upperdir=";
                opts += upperdir;
                opts += ",workdir=";
                opts += workerdir;
                
                // 0 - read-only
                // 1 - read-write default
                // 2 - read-only locked
                
                if (OVERLAY_MODE == 2 || xmount("overlay", tmp_mount.data(), "overlay", MS_RDONLY | MS_NOATIME, opts.data())) {
                    opts = "lowerdir=";
                    opts += upperdir;
                    opts += ":";
                    opts += get_lowerdirs(module_list, info.data());
                    opts += info.data();
                    if (xmount("overlay", tmp_mount.data(), "overlay", MS_RDONLY | MS_NOATIME, opts.data())) {
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
            break;
        }
    }

    LOGI("** Loading overlayfs\n");
    std::vector<string> mounted;
    for (auto &info : mount_list) {
        std::string tmp_mount = overlay_tmpdir + info;
        // OnePlus block mounting rw filesystem on /system, /vendor, etc...
        // https://github.com/lateautumn233/android_kernel_oneplus_sm8250/blob/2fb51004f2335f073a3ad6940a0abffdeccbe174/oplus/kernel/secureguard/rootguard/oplus_mount_block.c#L47
        if (xmount(tmp_mount.data(), info.data(), nullptr, MS_BIND | MS_REC | MS_RDONLY, nullptr)) {
            LOGE("mount failed, fall to mount subtree\n");
            // revert all mounts
            std::reverse(mounted.begin(), mounted.end());
            for (auto &dir : mounted) {
                umount2(dir.data(), MNT_DETACH);
            }
            mounted.clear();
            goto subtree_mounts; // fall back to mount subtree
        }
        mount("", info.data(), nullptr, MS_PRIVATE | MS_REC, nullptr);
        mount("", info.data(), nullptr, MS_SHARED | MS_REC, nullptr);
        mounted.emplace_back(info);
    }
    goto inject_mirrors;

    subtree_mounts:
    for (auto &info : mount_list) {
        DIR *dirfp;
        struct dirent *dp;
        char buf[4098];
        struct stat st;
        std::string tmp_mount = overlay_tmpdir + info;
        if ((dirfp = opendir(tmp_mount.data())) != nullptr) {
            while ((dp = readdir(dirfp)) != nullptr) {
                snprintf(buf, sizeof(buf) - 1, "%s/%s", tmp_mount.data(), dp->d_name);
                if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 ||
                lstat(buf, &st) != 0 || !S_ISDIR(st.st_mode))
                    continue;
                if (xmount(buf, buf + strlen(overlay_tmpdir.data()), nullptr, MS_BIND | MS_REC, nullptr)) {
                    LOGE("mount failed, skip!\n");
                    continue;
                }
                mount("", buf + strlen(overlay_tmpdir.data()), nullptr, MS_PRIVATE | MS_REC, nullptr);
                mount("", buf + strlen(overlay_tmpdir.data()), nullptr, MS_SHARED | MS_REC, nullptr);
                mounted.emplace_back(buf + strlen(overlay_tmpdir.data()));
            }
            closedir(dirfp);
        }
    }

    inject_mirrors: // inject mount back to to magisk mirrors so Magic mount won't override it
    if (mirrors != nullptr) {
        LOGI("** Loading overlayfs mirrors\n");
        for (auto &info : mounted) {
            std::string mirror_dir = string(mirrors) + info;
            if (access(mirror_dir.data(), F_OK) == 0) {
                xmount(info.data(), mirror_dir.data(), nullptr, MS_BIND | MS_REC, nullptr);
                mount("", mirror_dir.data(), nullptr, MS_PRIVATE | MS_REC, nullptr);
                mount("", mirror_dir.data(), nullptr, MS_SHARED | MS_REC, nullptr);
            }
        }
    }
    LOGI("mount done!\n");
    CLEANUP

    if (OVERLAY_MODE == 1) {
        do_remount(0, MS_RDONLY);
    }
    return 0;
}
