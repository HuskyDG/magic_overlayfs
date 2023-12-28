#include "base.hpp"
#include "logging.hpp"
#include "mountinfo.hpp"
#include "utils.hpp"
#include "partition.hpp"

using namespace std;

#define xmount(a,b,c,d,e) verbose_mount(a,b,c,d,e)
#define umount2(a,b) verbose_umount(a,b)

#define CLEANUP \
    LOGI("clean up\n"); \
    umount2(overlay_tmpdir.data(), MNT_DETACH); \
    rmdir(overlay_tmpdir.data());

#define TO_STR std::string("")

#define MNT_FLAGS (MS_LAZYTIME | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_DIRSYNC | MS_NOATIME | MS_NODIRATIME | MS_RELATIME | MS_STRICTATIME | MS_NOSYMFOLLOW | MS_MANDLOCK | MS_SILENT)

int log_fd = -1;
std::string overlay_tmpdir = "";

const char *overlay_name = "overlay";

static std::vector<mount_info> collect_mounts() {
    // sort mountinfo, skip unnecessary mounts
    std::vector<mount_info> mountinfo;
    auto current_mount_info = parse_mount_info("self");
    std::reverse(current_mount_info.begin(), current_mount_info.end());
    for (auto &info : current_mount_info) {
        for (auto &part : SYSTEM_PARTITIONS)
        if (starts_with(info.target.data(), string(part + "/").data()) || info.target == part) {
            for (auto &s : reversed(mountinfo)) {
                if (s.target == info.target || starts_with(info.target.data(), string(s.target + "/").data())) {
                    LOGD("mountinfo: mountpoint %s is hidden under %s\n", info.target.data(), s.target.data());
                    goto next_mountpoint;
                }
            }
            LOGD("mountinfo: device (%u:%u)%s on %s type %s (%s)\n", major(info.device), minor(info.device), 
                (info.root != "/")? info.root.data() : "", info.target.data(), info.type.data(), info.fs_option.data());
            mountinfo.emplace_back(info);
            break;
        }
        next_mountpoint:
        continue;
    }
    return mountinfo;
}

static int do_remount(int flags = 0, int exclude_flags = 0) {
    std::vector<mount_info> mountinfo = collect_mounts();
    struct statvfs stvfs{};
    exclude_flags |= MS_BIND;
    for (auto &mnt : mountinfo) {
        auto info = mnt.target;
        statvfs(info.data(), &stvfs);
        if (mnt.type == "overlay" || mnt.type == "tmpfs") {
            int fd = open(info.data(), O_PATH);
            string fd_path = "/proc/self/fd/";
            fd_path += std::to_string(fd);
            LOGD("%s [%s] (%s)\n", (mount(nullptr, fd_path.data(), nullptr, MS_REMOUNT | (stvfs.f_flag & MNT_FLAGS & ~exclude_flags) | flags, nullptr) == 0)?
                 "remounted success" : "remount failed", info.data(), mnt.type.data());
            close(fd);
        } else {
            LOGD("%s [%s] (%s)\n", "skipped", info.data(), mnt.type.data());
        }
    }
    return 0;
}

#include <set>

static int unmount_ksu_overlay() {
    std::vector<mount_info> mountinfo = collect_mounts();
    std::set<std::string> targets;
    for (auto &mnt : mountinfo) {
        if (mnt.source == "KSU") {
            targets.insert(mnt.target);
        }
    }
    if (targets.empty()) return 0;

    auto last_target = *targets.cbegin() + '/';
    for (auto iter = next(targets.cbegin()); iter != targets.cend();) {
        if (starts_with((*iter).data(), last_target.data())) {
            iter = targets.erase(iter);
        } else {
            last_target = *iter++ + '/';
        }
    }

    for (auto &s : targets) {
        mount(nullptr, s.data(), nullptr, MS_PRIVATE | MS_REC, nullptr);
        umount2(s.data(), MNT_DETACH);
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
    int init_mnt_ns = open("/proc/1/ns/mnt", O_RDONLY);
    if (init_mnt_ns >= 0 && setns(init_mnt_ns, 0) == 0) LOGI("Switched to init mount namespace\n");
    std::vector<mount_info> mountinfo = collect_mounts();

    const char *OVERLAY_MODE_env = xgetenv("OVERLAY_MODE");
    const char *OVERLAY_LEGACY_MOUNT_env = xgetenv("OVERLAY_LEGACY_MOUNT");
    const char *OVERLAYLIST_env = xgetenv("OVERLAYLIST");
    const char *MAGISKTMP_env = xgetenv("MAGISKTMP");

    if (OVERLAYLIST_env == nullptr) OVERLAYLIST_env = "";
    int OVERLAY_MODE = (OVERLAY_MODE_env)? atoi(OVERLAY_MODE_env) : 0;

    int32_t ksu_version = -1;
    prctl(0xdeadbeef, 2, &ksu_version, 0, 0);
    if (ksu_version >= 0) {
        LOGD("KernelSU (%d) is working\n", ksu_version);
    }
    {
        char buf[1024];
        int version_fd = open("/proc/version", O_RDONLY);
        if (read(version_fd, buf, sizeof(buf)-1) > 0) {
            char *linux_version = strstr(buf, "Linux version ");
            if (linux_version != nullptr) {
                int kmajor = 0, kminor = 0;
                sscanf(linux_version, "Linux version %d.%d", &kmajor, &kminor);
                LOGD("Kernel version: %d.%d\n", kmajor, kminor);
                /*
                if (ksu_version >= 10940 && OVERLAY_MODE == 2 && kmajor >= 5 && kminor >= 9) {
                    overlay_name = "KSU";
                    LOGD("Enabled KernelSU auto unmount\n");
                }
                */
            }
        }
        close(version_fd);
    }

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

    do {
        char *random_string = random_strc(20);
        overlay_tmpdir = std::string("/dev/.") + "worker_" + random_string;
        free(random_string);
    } while (mkdirs(overlay_tmpdir.data(), 750) != 0);
    std::string upper = std::string(argv[1]) + "/upper";
    std::string worker = std::string(argv[1]) + "/worker";
    mkdir(upper.data(), 0750);
    mkdirs((worker + "/empty").data(), 0750);

    xmount("tmpfs", overlay_tmpdir.data(), "tmpfs", 0, nullptr);
    mkdir(std::string(overlay_tmpdir + "/master").data(), 0750);

    if (!str_empty(OVERLAYLIST_env)) {
        std::string masterdir = overlay_tmpdir + "/master";
        std::string opts = "lowerdir=";
        opts += TO_STR + OVERLAYLIST_env + ":" + worker + "/empty";
        xmount(overlay_name, masterdir.data(), "overlay", 0, opts.data());
    }
    auto module_list = split_ro(OVERLAYLIST_env, ':');

    LOGI("** Prepare mounts\n");
    auto selinux_mode = getenforce();
    if (selinux_mode > 0) setenforce(0);
    // mount overlayfs for subdirectories of /system /vendor /product /system_ext
    std::reverse(mountinfo.begin(), mountinfo.end());
    for (auto &info : SYSTEM_PARTITIONS ) {
        struct stat st;
        struct statvfs FS_BUF;
        if (lstat(info.data(), &st) || !S_ISDIR(st.st_mode))
            continue;
        statvfs(info.data(), &FS_BUF);
        std::string tmp_mount = overlay_tmpdir + info;
        mkdirs(tmp_mount.data(), 0);

        std::string upperdir = upper + info;
        std::string workerdir = worker + "/" + std::to_string(st.st_dev) + "/" + std::to_string(st.st_ino);
        std::string masterdir = overlay_tmpdir + "/master" + info;
        char *con;
        {
            char *s = strdup(info.data());
            char *ss = s;
            while ((ss = strchr(ss, '/')) != nullptr) {
                ss[0] = '\0';
                auto sub = upper + s;
                if (mkdir(sub.data(), 0755) == 0 && getfilecon(s, &con) >= 0) {
                    int f_uid = getuidof(s), f_gid = getgidof(s), f_mode = getmod(s);
                    LOGD("clone attr context=[%s] uid=[%d] gid=[%d] mode=[%d] from [%s]\n",
                        con, f_uid, f_gid, f_mode, s);
                    if (chown(sub.data(), f_uid, f_gid) == -1) LOGE("chown failed [%s]\n", sub.data());
                    if (chmod(sub.data(), f_mode) == -1) LOGE("chmod failed [%s]\n", sub.data());
                    if (setfilecon(sub.data(), con) == -1) LOGE("setfilecon failed [%s]\n", sub.data());
                    freecon(con);
                }
                ss[0] = '/';
                ss++;
            }
            free(s);
        };
        
        {
            if (mkdir(upperdir.data(), 0755) == 0 && getfilecon(info.data(), &con) >= 0) {
                int f_uid = getuidof(info.data()), f_gid = getgidof(info.data()), f_mode = getmod(info.data());
                LOGD("clone attr context=[%s] uid=[%d] gid=[%d] mode=[%d] from [%s]\n",
                    con, f_uid, f_gid, f_mode, info.data());
                if (chown(upperdir.data(), f_uid, f_gid) == -1) LOGE("chown failed [%s]\n", upperdir.data());
                if (chmod(upperdir.data(), f_mode) == -1) LOGE("chmod failed [%s]\n", upperdir.data());
                if (setfilecon(upperdir.data(), con) == -1) LOGE("setfilecon failed [%s]\n", upperdir.data());
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
            std::string opts = TO_STR + 
                    "lowerdir=" +
                    get_lowerdirs(module_list, info.data()) +
                    info +
                    ",upperdir=" +
                    upperdir +
                    ",workdir=" +
                    workerdir;
            
            // 0 - read-only
            // 1 - read-write default
            // 2 - read-only locked
            
            if (OVERLAY_MODE == 2 || xmount(overlay_name, tmp_mount.data(), "overlay", MS_RDONLY | (FS_BUF.f_flag & MNT_FLAGS), opts.data())) {
                opts = TO_STR + 
                    "lowerdir=" +
                    upperdir +
                    ":" +
                    get_lowerdirs(module_list, info.data()) +
                    info;
                if (xmount(overlay_name, tmp_mount.data(), "overlay", MS_RDONLY | (FS_BUF.f_flag & MNT_FLAGS), opts.data())) {
                    LOGW("Unable to add [%s], ignore!\n", info.data());
                    continue;
                }
            }
            mount_list.push_back(info);
        }
    }

    // restore stock mounts if possible
    // if stock mount is directory, merge it with overlayfs
    // if stock mount is file, then we bind mount it back
    for (auto &mnt : mountinfo) {
        auto info = mnt.target;
        struct stat st;
        struct statvfs FS_BUF;
        if (stat(info.data(), &st))
            continue;
        statvfs(info.data(), &FS_BUF);
        std::string tmp_mount = overlay_tmpdir + info;
        std::string upperdir = upper + info;
        std::string workerdir = worker + "/" + std::to_string(st.st_dev) + "/" + std::to_string(st.st_ino);
        std::string masterdir = overlay_tmpdir + "/master" + info;
#define module_node_is_dir (is_dir(masterdir.data()))
#define module_node_exist (fexist(masterdir.data()))
#define upper_node_is_dir (is_dir(upperdir.data()))
#define upper_node_exist (fexist(upperdir.data()))
        if (lstat(tmp_mount.data(), &st) != 0 || // target does not exist, it could be deleted by modules
            (((upper_node_exist && !upper_node_is_dir) || // in upperdir, it is file
              (!upper_node_exist && module_node_exist && !module_node_is_dir)) && S_ISDIR(st.st_mode))) // module node is file but in lowerdir it is folder
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
                    auto sub = upper + s;
                    if (mkdir(sub.data(), 0755) == 0 && getfilecon(s, &con) >= 0) {
                        int f_uid = getuidof(s), f_gid = getgidof(s), f_mode = getmod(s);
                        LOGD("clone attr context=[%s] uid=[%d] gid=[%d] mode=[%d] from [%s]\n",
                             con, f_uid, f_gid, f_mode, s);
                        if (chown(sub.data(), f_uid, f_gid) == -1) LOGE("chown failed [%s]\n", sub.data());
                        if (chmod(sub.data(), f_mode) == -1) LOGE("chmod failed [%s]\n", sub.data());
                        if (setfilecon(sub.data(), con) == -1) LOGE("setfilecon failed [%s]\n", sub.data());
                        freecon(con);
                    }
                    ss[0] = '/';
                    ss++;
                }
                free(s);
            };
            {
                if (mkdir(upperdir.data(), 0755) == 0 && getfilecon(info.data(), &con) >= 0) {
                    int f_uid = getuidof(info.data()), f_gid = getgidof(info.data()), f_mode = getmod(info.data());
                    LOGD("clone attr context=[%s] uid=[%d] gid=[%d] mode=[%d] from [%s]\n",
                        con, f_uid, f_gid, f_mode, info.data());
                    if (chown(upperdir.data(), f_uid, f_gid) == -1) LOGE("chown failed [%s]\n", upperdir.data());
                    if (chmod(upperdir.data(), f_mode) == -1) LOGE("chmod failed [%s]\n", upperdir.data());
                    if (setfilecon(upperdir.data(), con) == -1) LOGE("setfilecon failed [%s]\n", upperdir.data());
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
                // parse to get context mount option
                auto mount_opts = split_ro(mnt.fs_option, ',');
                std::string context_opt = "";
                for (auto &mnt_opt : mount_opts) {
                   if (starts_with(mnt_opt.data(), "context=")) {
                       context_opt = mnt_opt;
                       break;
                   }
                }
                std::string opts = TO_STR +
                        "lowerdir=" +
                        get_lowerdirs(module_list, info.data()) +
                        info +
                        ",upperdir=" +
                        upperdir +
                        ",workdir=" +
                        workerdir;
                if (!str_empty(context_opt.data()))
                    opts += TO_STR + "," + context_opt;
                    // 0 - read-only
                    // 1 - read-write default
                    // 2 - read-only locked
                
                if (OVERLAY_MODE == 2 || xmount(overlay_name, tmp_mount.data(), "overlay", MS_RDONLY | (FS_BUF.f_flag & MNT_FLAGS), opts.data())) {
                    opts = TO_STR +
                            "lowerdir=" +
                            upperdir +
                            ":" +
                            get_lowerdirs(module_list, info.data()) +
                            info;
                    if (!str_empty(context_opt.data()))
                        opts += TO_STR + "," + context_opt;
                    if (xmount(overlay_name, tmp_mount.data(), "overlay", MS_RDONLY | (FS_BUF.f_flag & MNT_FLAGS), opts.data())) {
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
    if (selinux_mode > 0) setenforce(selinux_mode);

    LOGI("** Loading overlayfs\n");
    std::vector<string> mounted;
    if (OVERLAY_LEGACY_MOUNT_env && OVERLAY_LEGACY_MOUNT_env == string_view("true"))
        goto subtree_mounts;
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
