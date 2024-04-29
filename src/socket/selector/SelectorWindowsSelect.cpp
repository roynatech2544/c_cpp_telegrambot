#include <absl/log/log.h>
#include <winsock2.h>
#include <algorithm>

#include "SelectorWindows.hpp"
#include <socket/interface/impl/SocketWindows.hpp>

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
    struct timeval tv {
        .tv_sec = 5
    };
    bool any = false;

    int ret = select(FD_SETSIZE, &set, nullptr, nullptr, &tv);
    if (ret == SOCKET_ERROR) {
        WSALOG_E("Select failed");
        return SelectorPollResult::ERROR_GENERIC;
    }
    for (auto &e : data) {
        if (FD_ISSET(e.fd, &set)) {
            e.callback();
            any = true;
        }
    }
    if (!any) {
        return SelectorPollResult::ERROR_NOTHING_FOUND;
    } else {
        return SelectorPollResult::OK;
    }
}

void SelectSelector::shutdown() {}