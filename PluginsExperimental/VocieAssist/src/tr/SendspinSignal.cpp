#include "tr/SendspinSignal.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag      = "sendspin";
constexpr const char* kProcName = "sendspin-client";

pid_t FindProcByName(const char* name) {
    DIR* d = ::opendir("/proc");
    if (d == nullptr) return -1;
    pid_t result = -1;
    struct dirent* ent = nullptr;
    while ((ent = ::readdir(d)) != nullptr) {
        if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) continue;

        // Filename must be all digits to be a pid.
        bool all_digits = true;
        for (const char* p = ent->d_name; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') { all_digits = false; break; }
        }
        if (!all_digits || ent->d_name[0] == '\0') continue;

        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
        int fd = ::open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char buf[64];
        const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        ::close(fd);
        if (n <= 0) continue;
        buf[n] = '\0';
        // /proc/<pid>/comm has a trailing newline.
        if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
        if (std::strcmp(buf, name) == 0) {
            result = static_cast<pid_t>(std::atoi(ent->d_name));
            break;
        }
    }
    ::closedir(d);
    return result;
}

void SendSignal(int sig, const char* sig_name) {
    const pid_t pid = FindProcByName(kProcName);
    if (pid <= 0) {
        LVA_LOGD(kTag, "%s not running, skipping %s",
                 kProcName, sig_name);
        return;
    }
    if (::kill(pid, sig) != 0) {
        LVA_LOGW(kTag, "kill(%d, %s) failed: %s",
                 (int)pid, sig_name, std::strerror(errno));
        return;
    }
    LVA_LOGD(kTag, "%s -> pid %d", sig_name, (int)pid);
}

}  // namespace

void SendspinDuck()   { SendSignal(SIGUSR1, "SIGUSR1"); }
void SendspinUnduck() { SendSignal(SIGUSR2, "SIGUSR2"); }

}  // namespace lva::tr
