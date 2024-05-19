#include <CStringLifetime.h>
#include <absl/log/log.h>
#include <socket/TgBotSocket.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <impl/SocketPosix.hpp>

#include "SocketBase.hpp"
#include "SocketDescriptor_defs.hpp"

bool SocketInterfaceUnixLocal::createLocalSocket(SocketConnContext *ctx) {
    ctx->cfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->cfd < 0) {
        PLOG(ERROR) << "Failed to create socket";
        return false;
    }
    auto *addr = reinterpret_cast<struct sockaddr_un *>(ctx->addr.get());
    addr->sun_family = AF_LOCAL;
    strncpy(addr->sun_path, getOptions(Options::DESTINATION_ADDRESS).c_str(),
            sizeof(addr->sun_path));
    addr->sun_path[sizeof(addr->sun_path) - 1] = '\0';
    ctx->len = SUN_LEN(addr);
    return true;
}

std::optional<socket_handle_t> SocketInterfaceUnixLocal::createServerSocket() {
    SocketConnContext ret = SocketConnContext::create<sockaddr_un>();
    const auto *_name = reinterpret_cast<struct sockaddr *>(ret.addr.get());

    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    LOG(INFO) << "Creating socket at " << getSocketPath().string();
    if (!createLocalSocket(&ret)) {
        return std::nullopt;
    }
    if (bind(ret.cfd, _name, ret.len) != 0) {
        bool succeeded = false;
        PLOG(ERROR) << "Failed to bind to socket";
        if (errno == EADDRINUSE) {
            cleanupServerSocket();
            if (bind(ret.cfd, _name, ret.len) == 0) {
                LOG(INFO) << "Bind succeeded by removing socket file";
                succeeded = true;
            }
        }
        if (!succeeded) {
            closeSocketHandle(ret.cfd);
            return std::nullopt;
        }
    }
    return ret.cfd;
}

std::optional<SocketConnContext> SocketInterfaceUnixLocal::createClientSocket() {
    SocketConnContext ret = SocketConnContext::create<sockaddr_un>();
    const auto *_name = reinterpret_cast<struct sockaddr *>(ret.addr.get());

    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    if (!createLocalSocket(&ret)) {
        return std::nullopt;
    }
    if (connect(ret.cfd, _name, ret.len) != 0) {
        PLOG(ERROR) << "Failed to connect to socket";
        closeSocketHandle(ret.cfd);
    }
    return ret;
}

void SocketInterfaceUnixLocal::cleanupServerSocket() {
    helper.local.cleanupServerSocket();
}

bool SocketInterfaceUnixLocal::canSocketBeClosed() {
    return helper.local.canSocketBeClosed();
}

bool SocketInterfaceUnixLocal::isSupported() {
    return helper.local.isSupported();
}

void SocketInterfaceUnixLocal::doGetRemoteAddr(socket_handle_t s) {
    helper.local.doGetRemoteAddr(s);
}