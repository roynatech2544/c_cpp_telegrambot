#include "../SocketInterfaceWindows.h"
#include "socket/SocketInterfaceBase.h"

bool SocketHelperWindows::createInetSocketAddr(socket_handle_t *sfd,
                                               struct sockaddr_in *addr,
                                               SocketInterfaceBase *it) {
    *sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!SocketInterfaceWindows::isValidSocketHandle(*sfd)) {
        WSALOG_E("Failed to create socket");
        return false;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(SocketHelperCommon::getPortNumInet(it));
    return true;
}

bool SocketHelperWindows::createInet6SocketAddr(socket_handle_t *sfd,
                                                struct sockaddr_in6 *addr,
                                                SocketInterfaceBase *it) {
    *sfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (!SocketInterfaceWindows::isValidSocketHandle(*sfd)) {
        WSALOG_E("Failed to create socket");
        return false;
    }

    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(SocketHelperCommon::getPortNumInet(it));
    return true;
};