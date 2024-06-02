#include <absl/log/log.h>
#include <sys/select.h>

#include "SelectorPosix.hpp"

bool SelectSelector::init() {
    FD_ZERO(&set);
    return true;
}

bool SelectSelector::add(socket_handle_t fd, OnSelectedCallback callback) {
    if (FD_ISSET(fd, &set)) {
        LOG(WARNING) << "fd " << fd << " already set";
        return false;
    }
    FD_SET(fd, &set);
    data.push_back({fd, callback});
    return true;
}

bool SelectSelector::remove(socket_handle_t fd) {
    bool ret = false;
    std::erase_if(data, [fd, &ret](const SelectFdData &e) {
        if (e.fd == fd) {
            ret = true;
            return true;
        }
        return false;
    });
    return ret;
}

SelectSelector::SelectorPollResult SelectSelector::poll() {
    int ret = select(FD_SETSIZE, &set, nullptr, nullptr, nullptr);
    if (ret < 0) {
        PLOG(ERROR) << "Select failed";
        return SelectorPollResult::FAILED;
    }
    for (auto &e : data) {
        if (FD_ISSET(e.fd, &set)) {
            e.callback();
        }
    }
    // We have used the select: reinit them
    reinit();
    return SelectorPollResult::OK;
}

void SelectSelector::shutdown() {}

bool SelectSelector::reinit() {
    init();
    for (auto &e : data) {
        FD_SET(e.fd, &set);
    }
    return true;
}