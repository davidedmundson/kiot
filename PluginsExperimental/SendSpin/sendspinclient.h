#pragma once

#include <string>
#include <thread>
#include <atomic>

class SendSpinClientWrapper {
public:
    SendSpinClientWrapper();
    ~SendSpinClientWrapper();

    bool start(const std::string& connect_url = "");
    void stop();

private:
    void run_client(std::string connect_url);

    std::thread worker_thread_;
    std::atomic<bool> running_{false};
};