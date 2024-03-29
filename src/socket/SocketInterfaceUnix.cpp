#include "SocketInterfaceUnix.h"

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Logging.h"

void SocketInterfaceUnix::startListening(const listener_callback_t& listen_cb,
                                         const result_callback_t& result_cb) {
    bool should_break = false, value_set = false;
    int rc = 0;
    socket_handle_t sfd = createServerSocket();
    if (isValidSocketHandle(sfd)) {
        do {
            if (listen(sfd, 1) < 0) {
                PLOG_E("Failed to listen to socket");
                break;
            }
            rc = pipe(kListenTerminate);
            if (rc < 0) {
                PLOG_E("Pipe failed");
                break;
            }
            result_cb(true);
            value_set = true;
            while (!should_break) {
                struct sockaddr_un addr {};
                struct TgBotConnection conn {};
                socklen_t len = sizeof(addr);
                struct pollfd fds[] = {{
                                           .fd = listen_fd,
                                           .events = POLLIN,
                                           .revents = 0,
                                       },
                                       {
                                           .fd = sfd,
                                           .events = POLLIN,
                                           .revents = 0,

                                       }};
                const pollfd& listen_fd_poll = fds[0];
                const pollfd& socket_fd_poll = fds[1];

                LOG(LogLevel::DEBUG, "Waiting for incoming events");

                rc = poll(fds, sizeof(fds) / sizeof(pollfd), -1);
                if (rc < 0) {
                    PLOG_E("Poll failed");
                    break;
                }

                if (rc == 2) {
                    LOG(LogLevel::WARNING, "Dropping incoming buffer: exiting");
                }
                if (listen_fd_poll.revents & POLLIN) {
                    dummy_listen_buf_t buf;
                    ssize_t rc =
                        read(listen_fd, &buf, sizeof(dummy_listen_buf_t));
                    if (rc < 0) PLOG_E("Reading data from forcestop fd");
                    closeFd(listen_fd);
                    break;
                } else if (!(socket_fd_poll.revents & POLLIN)) {
                    LOG(LogLevel::ERROR,
                        "Unexpected state: sfd.revents: %d, listen_fd.revents: "
                        "%d",
                        socket_fd_poll.revents, listen_fd_poll.revents);
                    break;
                }

                const socket_handle_t cfd =
                    accept(sfd, (struct sockaddr*)&addr, &len);

                if (!isValidSocketHandle(cfd)) {
                    PLOG_E("Accept failed");
                    break;
                } else {
                    doGetRemoteAddr(cfd);
                }
                const int count = read(cfd, &conn, sizeof(conn));
                should_break = handleIncomingBuf(
                    count, conn, listen_cb, [] { return strerror(errno); });
                close(cfd);
            }
        } while (false);
        closePipe(kListenTerminate);
        close(sfd);
        cleanupServerSocket();
    }
    if (!value_set) result_cb(false);
}

void SocketInterfaceUnix::writeToSocket(struct TgBotConnection conn) {
    const socket_handle_t sfd = createClientSocket();
    if (isValidSocketHandle(sfd)) {
        const int count = write(sfd, &conn, sizeof(conn));
        if (count < 0) {
            PLOG_E("Failed to write to socket");
        }
        close(sfd);
    }
}

void SocketInterfaceUnix::forceStopListening(void) {
    if (isValidFd(notify_fd) && isValidSocketHandle(listen_fd)) {
        dummy_listen_buf_t d = {};
        int count;

        count = write(notify_fd, &d, sizeof(dummy_listen_buf_t));
        if (count < 0) {
            PLOG_E("Failed to write to notify pipe");
        }
        closeFd(notify_fd);
    }
}

std::shared_ptr<SocketInterfaceBase> SocketInterfaceGetter::getForClient() {
    static const std::vector<std::shared_ptr<SocketInterfaceBase>>
        socket_interfaces_client{
            std::make_shared<SocketInterfaceUnixIPv4>(),
            std::make_shared<SocketInterfaceUnixIPv6>(),
            std::make_shared<SocketInterfaceUnixLocal>(),
        };

    for (const auto& e : socket_interfaces_client) {
        if (e->isAvailable()) {
            return e;
        }
    }
    return nullptr;
}

std::shared_ptr<SocketInterfaceBase> SocketInterfaceGetter::get(
    const SocketNetworkType type, const SocketUsage usage) {
    const auto tusage =
        static_cast<SingleThreadCtrlManager::ThreadUsage>(usage);
    std::shared_ptr<SocketInterfaceBase> ptr;
    auto& mgr = SingleThreadCtrlManager::getInstance();
    switch (type) {
        case SocketNetworkType::TYPE_IPV4:
            ptr = mgr.getController<SocketInterfaceUnixIPv4>(tusage);
        case SocketNetworkType::TYPE_IPV6:
            ptr = mgr.getController<SocketInterfaceUnixIPv6>(tusage);
        case SocketNetworkType::TYPE_LOCAL_UNIX:
            ptr = mgr.getController<SocketInterfaceUnixLocal>(tusage);
    };
    return ptr;
}