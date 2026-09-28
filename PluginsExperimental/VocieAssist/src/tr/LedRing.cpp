#include "tr/LedRing.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "util/Log.h"

extern char** environ;

namespace lva::tr {

namespace {

constexpr const char* kTag    = "led";
constexpr const char* kAnimDir = "/usr/share/thirdreality/animation/";

// Mirrors the Python build's _LED_ANIMATIONS map exactly.
const char* AnimationFor(LedState s) {
    switch (s) {
        case LedState::Listening: return "active-waking.animation";
        case LedState::Thinking:  return "active-thinking.animation";
        case LedState::Speaking:  return "active-talking.animation";
        case LedState::Idle:      return "active-ending.animation";
        case LedState::Error:     return "active-ending.animation";
        case LedState::Muted:     return "mics-off_on.animation";
        case LedState::Unmuted:   return "none.animation";
    }
    return nullptr;
}

bool IsTerminal(LedState s) {
    // "to_idle=true" cases in Python: muted, unmuted, idle, error.
    switch (s) {
        case LedState::Idle:
        case LedState::Error:
        case LedState::Muted:
        case LedState::Unmuted:
            return true;
        default:
            return false;
    }
}

}  // namespace

void Show(LedState state) {
    const char* anim_name = AnimationFor(state);
    if (anim_name == nullptr) {
        LVA_LOGW(kTag, "unknown LED state");
        return;
    }
    std::string anim_path = std::string(kAnimDir) + anim_name;

    const char* to_idle_str = IsTerminal(state) ? "boolean:true"
                                                : "boolean:false";
    std::string array_arg = std::string("array:string:") + anim_path;

    char prog[]    = "/usr/bin/dbus-send";
    char system_a[]= "--system";
    char type_a[]  = "--type=signal";
    char path_a[]  = "/com/3r/EventBus";
    char iface_a[] = "com._3reality.EventBus.LedShow";
    char to_idle_buf[24];
    std::strncpy(to_idle_buf, to_idle_str, sizeof(to_idle_buf) - 1);
    to_idle_buf[sizeof(to_idle_buf) - 1] = '\0';
    std::string array_buf = array_arg;

    char* argv[] = {
        prog, system_a, type_a, path_a, iface_a,
        to_idle_buf, array_buf.data(), nullptr,
    };

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
        LVA_LOGW(kTag, "posix_spawn failed: %s", std::strerror(rc));
        return;
    }
    LVA_LOGD(kTag, "show '%s' (pid=%d)", anim_name, (int)pid);
}

void ShowVolumeChanged() {
    std::string anim_path =
        std::string(kAnimDir) + "volume-changed.animation";
    std::string array_arg = std::string("array:string:") + anim_path;

    char prog[]    = "/usr/bin/dbus-send";
    char system_a[]= "--system";
    char type_a[]  = "--type=signal";
    char path_a[]  = "/com/3r/EventBus";
    char iface_a[] = "com._3reality.EventBus.LedShow";
    char to_idle[] = "boolean:false";
    std::string array_buf = array_arg;

    char* argv[] = {
        prog, system_a, type_a, path_a, iface_a,
        to_idle, array_buf.data(), nullptr,
    };

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
        LVA_LOGW(kTag, "posix_spawn failed: %s", std::strerror(rc));
        return;
    }
    LVA_LOGD(kTag, "show 'volume-changed' (pid=%d)", (int)pid);
}

}  // namespace lva::tr
