#include "tr/SystemVolume.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <pulse/volume.h>

#include "util/Log.h"

extern char** environ;

namespace lva::tr {

namespace {

constexpr const char* kTag = "trvol";

void FormatPaVolume(int percent, char* buf, size_t buf_size) {
    const double linear = std::clamp(percent, 0, 100) / 100.0;
    const pa_volume_t pa_vol = pa_sw_volume_from_linear(linear);
    std::snprintf(buf, buf_size, "%u", (unsigned)pa_vol);
}

}  // namespace

void SetSystemVolume(int percent) {
    const int clamped = std::clamp(percent, 0, 100);
    char arg_buf[16];
    FormatPaVolume(clamped, arg_buf, sizeof(arg_buf));

    char prog[]   = "/usr/bin/pactl";
    char a1[]     = "set-sink-volume";
    char a2[]     = "@DEFAULT_SINK@";
    char* argv[]  = { prog, a1, a2, arg_buf, nullptr };

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                     "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                     "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                     "/dev/null", O_WRONLY, 0);

    pid_t pid = 0;
    const int rc = ::posix_spawn(&pid, prog, &actions, nullptr,
                                 argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        LVA_LOGW(kTag, "pactl spawn failed: %s", std::strerror(rc));
        return;
    }
    LVA_LOGI(kTag, "SetVolume %d%% -> pa_vol=%s (pid=%d)",
             clamped, arg_buf, (int)pid);
}

void SetSystemVolumeSilent(int percent) {
    const int clamped = std::clamp(percent, 0, 100);
    char arg_buf[16];
    FormatPaVolume(clamped, arg_buf, sizeof(arg_buf));

    char prog[]   = "/usr/bin/pactl";
    char a1[]     = "set-sink-volume";
    char a2[]     = "@DEFAULT_SINK@";
    char* argv[]  = { prog, a1, a2, arg_buf, nullptr };

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                     "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                     "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                     "/dev/null", O_WRONLY, 0);

    pid_t pid = 0;
    const int rc = ::posix_spawn(&pid, prog, &actions, nullptr,
                                 argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        LVA_LOGW(kTag, "pactl spawn failed: %s", std::strerror(rc));
        return;
    }
    LVA_LOGI(kTag, "SetVolumeSilent %d%% -> pa_vol=%s (pid=%d)",
             clamped, arg_buf, (int)pid);
}

}  // namespace lva::tr
