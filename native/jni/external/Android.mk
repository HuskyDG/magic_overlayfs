LOCAL_PATH := $(call my-dir)
SE_PATH := $(LOCAL_PATH)/selinux

# libselinux.a
include $(CLEAR_VARS)
LIBSELINUX := $(SE_PATH)/libselinux/include
LOCAL_MODULE:= libselinux
LOCAL_C_INCLUDES := $(LIBSELINUX)
LOCAL_EXPORT_C_INCLUDES := $(LIBSELINUX)
LOCAL_STATIC_LIBRARIES := libpcre2
LOCAL_CFLAGS := \
    -Wno-implicit-function-declaration -Wno-int-conversion -Wno-unused-function \
    -Wno-macro-redefined -Wno-unused-but-set-variable -D_GNU_SOURCE -DUSE_PCRE2 \
    -DNO_PERSISTENTLY_STORED_PATTERNS -DDISABLE_SETRANS -DDISABLE_BOOL \
    -DNO_MEDIA_BACKEND -DNO_X_BACKEND -DNO_DB_BACKEND -DNO_ANDROID_BACKEND \
    -Dfgets_unlocked=fgets -D'__fsetlocking(...)='
LOCAL_SRC_FILES := \
    selinux/libselinux/src/avc.c \
    selinux/libselinux/src/avc_internal.c \
    selinux/libselinux/src/avc_sidtab.c \
    selinux/libselinux/src/booleans.c \
    selinux/libselinux/src/callbacks.c \
    selinux/libselinux/src/canonicalize_context.c \
    selinux/libselinux/src/checkAccess.c \
    selinux/libselinux/src/check_context.c \
    selinux/libselinux/src/checkreqprot.c \
    selinux/libselinux/src/compute_av.c \
    selinux/libselinux/src/compute_create.c \
    selinux/libselinux/src/compute_member.c \
    selinux/libselinux/src/compute_relabel.c \
    selinux/libselinux/src/compute_user.c \
    selinux/libselinux/src/context.c \
    selinux/libselinux/src/deny_unknown.c \
    selinux/libselinux/src/disable.c \
    selinux/libselinux/src/enabled.c \
    selinux/libselinux/src/fgetfilecon.c \
    selinux/libselinux/src/freecon.c \
    selinux/libselinux/src/freeconary.c \
    selinux/libselinux/src/fsetfilecon.c \
    selinux/libselinux/src/get_context_list.c \
    selinux/libselinux/src/get_default_type.c \
    selinux/libselinux/src/get_initial_context.c \
    selinux/libselinux/src/getenforce.c \
    selinux/libselinux/src/getfilecon.c \
    selinux/libselinux/src/getpeercon.c \
    selinux/libselinux/src/init.c \
    selinux/libselinux/src/is_customizable_type.c \
    selinux/libselinux/src/label.c \
    selinux/libselinux/src/label_file.c \
    selinux/libselinux/src/label_support.c \
    selinux/libselinux/src/lgetfilecon.c \
    selinux/libselinux/src/load_policy.c \
    selinux/libselinux/src/lsetfilecon.c \
    selinux/libselinux/src/mapping.c \
    selinux/libselinux/src/matchmediacon.c \
    selinux/libselinux/src/matchpathcon.c \
    selinux/libselinux/src/policyvers.c \
    selinux/libselinux/src/procattr.c \
    selinux/libselinux/src/query_user_context.c \
    selinux/libselinux/src/regex.c \
    selinux/libselinux/src/reject_unknown.c \
    selinux/libselinux/src/selinux_check_securetty_context.c \
    selinux/libselinux/src/selinux_config.c \
    selinux/libselinux/src/selinux_restorecon.c \
    selinux/libselinux/src/sestatus.c \
    selinux/libselinux/src/setenforce.c \
    selinux/libselinux/src/setexecfilecon.c \
    selinux/libselinux/src/setfilecon.c \
    selinux/libselinux/src/setrans_client.c \
    selinux/libselinux/src/seusers.c \
    selinux/libselinux/src/sha1.c \
    selinux/libselinux/src/stringrep.c \
    selinux/libselinux/src/validatetrans.c
include $(BUILD_STATIC_LIBRARY)


# libpcre2.a
include $(CLEAR_VARS)
LIBPCRE2 := $(LOCAL_PATH)/pcre/include
LOCAL_MODULE:= libpcre2
LOCAL_CFLAGS := -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=8
LOCAL_C_INCLUDES := $(LIBPCRE2) $(LIBPCRE2)_internal
LOCAL_EXPORT_C_INCLUDES := $(LIBPCRE2)
LOCAL_SRC_FILES := \
    pcre/src/pcre2_auto_possess.c \
    pcre/src/pcre2_compile.c \
    pcre/src/pcre2_config.c \
    pcre/src/pcre2_context.c \
    pcre/src/pcre2_convert.c \
    pcre/src/pcre2_dfa_match.c \
    pcre/src/pcre2_error.c \
    pcre/src/pcre2_extuni.c \
    pcre/src/pcre2_find_bracket.c \
    pcre/src/pcre2_fuzzsupport.c \
    pcre/src/pcre2_maketables.c \
    pcre/src/pcre2_match.c \
    pcre/src/pcre2_match_data.c \
    pcre/src/pcre2_jit_compile.c \
    pcre/src/pcre2_newline.c \
    pcre/src/pcre2_ord2utf.c \
    pcre/src/pcre2_pattern_info.c \
    pcre/src/pcre2_script_run.c \
    pcre/src/pcre2_serialize.c \
    pcre/src/pcre2_string_utils.c \
    pcre/src/pcre2_study.c \
    pcre/src/pcre2_substitute.c \
    pcre/src/pcre2_substring.c \
    pcre/src/pcre2_tables.c \
    pcre/src/pcre2_ucd.c \
    pcre/src/pcre2_valid_utf.c \
    pcre/src/pcre2_xclass.c \
    pcre2_workaround.c
include $(BUILD_STATIC_LIBRARY)