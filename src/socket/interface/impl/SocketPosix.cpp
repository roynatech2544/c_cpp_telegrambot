#include "SocketPosix.hpp"

#include <absl/log/log.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <socket/selector/SelectorPosix.hpp>

void SocketInterfaceUnix::startListening(socket_handle_t handle,
                                         const listener_callback_t onNewData) {
    bool should_break = false;
    int rc = 0;
    UnixSelector selector;

    if (!isValidSocketHandle(handle)) {
        return;
    }

    do {
        if (listen(handle, SOMAXCONN) < 0) {
            PLOG(ERROR) << "Failed to listen to socket";
            break;
        }
        if (!kListenTerminate.pipe()) {
            PLOG(ERROR) << "Failed to create pipe";
            break;
        }
        if (!selector.init()) {
            break;
        }
        selector.add(kListenTerminate.readEnd(), [this, &should_break]() {
            dummy_listen_buf_t buf = {};
            ssize_t rc = read(kListenTerminate.readEnd(), &buf, sizeof(dummy_listen_buf_t));
            if (rc < 0) {
                PLOG(ERROR) << "Reading data from forcestop fd";
            }
            close(kListenTerminate.readEnd());
            should_break = true;
        });
        selector.add(handle, [handle, this, &should_break, onNewData] {
            struct sockaddr addr {};
            socklen_t len = sizeof(addr);
            socket_handle_t cfd = accept(handle, &addr, &len);

            if (!isValidSocketHandle(cfd)) {
                PLOG(ERROR) << "Accept failed";
            } else {
                doGetRemoteAddr(cfd);
                SocketConnContext ctx(addr);
                ctx.cfd = cfd;
                should_break = onNewData(ctx);
                closeSocketHandle(cfd);
            }
        });
        while (!should_break) {
            switch (selector.poll()) {
                case Selector::SelectorPollResult::FAILED:
                    should_break = true;
                    break;
                case Selector::SelectorPollResult::OK:
                case Selector::SelectorPollResult::TIMEOUT:
                    break;
            }
        }
        selector.shutdown();
    } while (false);
    kListenTerminate.close();
    closeSocketHandle(handle);
    cleanupServerSocket();
}

void SocketInterfaceUnix::forceStopListening() {
    if (kListenTerminate.isVaild()) {
        dummy_listen_buf_t d = {};
        ssize_t count = 0;
        int notify_fd = kListenTerminate.writeEnd();

        count = write(notify_fd, &d, sizeof(dummy_listen_buf_t));
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to notify pipe";
        }
        closeSocketHandle(notify_fd);
    }
}

bool SocketInterfaceUnix::writeToSocket(SocketConnContext context,
                                        SharedMalloc data) {
    auto* socketData = static_cast<char*>(data.get());
    auto* addr = static_cast<sockaddr*>(context.addr.get());
    if (isValidSocketHandle(context.cfd)) {
        const auto count =
            send(context.cfd, socketData, data->size, MSG_NOSIGNAL);
        if (count < 0) {
            PLOG(ERROR) << "Failed to send to socket";
            return false;
        }
    } else {
        const auto count = sendto(context.cfd, socketData, data->size,
                                  MSG_NOSIGNAL, addr, context.addr->size);
        if (count < 0) {
            PLOG(ERROR) << "Failed to sentto socket";
            return false;
        }
    }
    return true;
}

std::optional<SharedMalloc> SocketInterfaceUnix::readFromSocket(
    SocketConnContext handle, buffer_len_t length) {
    SharedMalloc buf(length);
    auto* addr = static_cast<sockaddr*>(handle.addr.get());
    socklen_t addrlen = handle.addr->size;

    ssize_t count = recvfrom(handle.cfd, buf.get(), length,
                             MSG_NOSIGNAL | MSG_WAITALL, addr, &addrlen);
    if (count != length) {
        PLOG(ERROR) << "Failed to read from socket";
    } else {
        return buf;
    }
    return std::nullopt;
}

bool SocketInterfaceUnix::closeSocketHandle(socket_handle_t& handle) {
    if (isValidSocketHandle(handle)) {
        closeFd(handle);
        handle = kInvalidFD;
        return true;
    }
    return false;
}

bool SocketInterfaceUnix::setSocketOptTimeout(socket_handle_t handle,
                                              int timeout) {
    struct timeval tv {
        .tv_sec = timeout
    };
    int ret = 0;

    ret |= setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ret |= setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (ret) {
        PLOG(ERROR) << "Failed to set socket timeout";
    }
    return ret == 0;
}
