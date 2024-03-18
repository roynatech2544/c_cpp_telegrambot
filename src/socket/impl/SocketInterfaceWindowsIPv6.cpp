#include "../SocketInterfaceWindows.h"
#include "socket/SocketInterfaceBase.h"

socket_handle_t SocketInterfaceWindowsIPv6::createServerSocket() {
    struct sockaddr_in6 name {};
    socket_handle_t sfd;

    if (SocketHelperWindows::createInet6SocketAddr(&sfd, &name)) {
        name.sin6_addr = in6addr_any;
        if (bind(sfd, reinterpret_cast<struct sockaddr *>(&name),
                 sizeof(name)) != 0) {
            WSALOG_E("Failed to bind to socket");
            closesocket(sfd);
            return INVALID_SOCKET;
        }
    }
    return sfd;
}

socket_handle_t SocketInterfaceWindowsIPv6::createClientSocket() {
    struct sockaddr_in6 name {};
    socket_handle_t sfd;

    if (SocketHelperWindows::createInet6SocketAddr(&sfd, &name)) {
        InetPton(AF_INET6, getOptions(Options::DESTINATION_ADDRESS).c_str(),
                 &name.sin6_addr);
        if (connect(sfd, reinterpret_cast<struct sockaddr *>(&name),
                    sizeof(name)) != 0) {
            WSALOG_E("Failed to connect to socket");
            closesocket(sfd);
            return INVALID_SOCKET;
        }
    }
    return sfd;
}

bool SocketInterfaceWindowsIPv6::isAvailable() {
    return SocketHelperCommon::isAvailableIPv6(this);
}

void SocketInterfaceWindowsIPv6::stopListening(const std::string& e) {
    forceStopListening();
}