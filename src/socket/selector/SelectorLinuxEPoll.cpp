#include <internal/_FileDescriptor_posix.h>
#include <sys/epoll.h>

#include <algorithm>

#include "SelectorPosix.hpp"
#include "SocketDescriptor_defs.hpp"

bool EPollSelector::init() { return (isValidFd(epollfd = epoll_create(MAX_EPOLLFDS))); }

bool EPollSelector::add(socket_handle_t fd, OnSelectedCallback callback) {
    struct epoll_event event {};

    if (data.size() > MAX_EPOLLFDS) {
        LOG(WARNING) << "epoll fd buffer full";
        return false;
    }
    
    if (std::ranges::any_of(data,
                            [fd](const auto &data) { return data.fd == fd; })) {
        LOG(WARNING) << "epoll fd " << fd << " is already added";
        return false;
    }

    event.data.fd = fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) < 0) {
        PLOG(ERROR) << "epoll_ctl failed";
        return false;
    }
    data.emplace_back(fd, callback);
    return true;
}

bool EPollSelector::remove(socket_handle_t fd) {
    bool found = std::ranges::any_of(
        data, [fd](const auto &data) { return data.fd == fd; });
    if (found) {
        std::ranges::remove_if(
            data, [fd](const auto &data) { return data.fd == fd; });
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            PLOG(ERROR) << "epoll_ctl failed";
            return false;
        }
    } else {
        LOG(WARNING) << "epoll fd " << fd << " is not added";
        return false;
    }
    return found;
}

EPollSelector::SelectorPollResult EPollSelector::poll() {
    epoll_event result_event{};
    int result = epoll_wait(epollfd, &result_event, 1, -1);
    if (result < 0) {
        PLOG(ERROR) << "epoll_wait failed";
        return SelectorPollResult::FAILED;
    } else if (result > 0) {
        for (const auto &data : data) {
            if (data.fd == result_event.data.fd) {
                data.callback();
                break;
            }
        }
    }
    return SelectorPollResult::OK;
}

void EPollSelector::shutdown() {
    if (isValidFd(epollfd)) {
        ::closeFd(epollfd);
    }
}

bool EPollSelector::reinit() {
    return true;
}