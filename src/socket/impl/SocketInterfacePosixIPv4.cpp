#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <functional>

// TgBot deps
#include <Logging.h>
#include <Types.h>

#include "../SocketInterfaceUnix.h"
#include "socket/SocketInterfaceBase.h"

void SocketInterfaceUnixIPv4::foreach_ipv4_interfaces(
    const std::function<void(const char*, const char*)> callback) {
    struct ifaddrs *addrs, *tmp;
    getifaddrs(&addrs);
    tmp = addrs;
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* pAddr = (struct sockaddr_in*)tmp->ifa_addr;
            callback(tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
}

socket_handle_t SocketInterfaceUnixIPv4::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in name {};
    const socket_handle_t sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG_E("Failed to create socket");
        return ret;
    }

    LOG(LogLevel::DEBUG, "Dump of active interfaces' addresses (IPv4)");
    foreach_ipv4_interfaces([](const char* iface, const char* addr) {
        LOG(LogLevel::DEBUG, "ifname %s: addr %s", iface, addr);
    });
    foreach_ipv4_interfaces(
        [&iface_done, sfd](const char* iface, const char* addr) {
            if (!iface_done && strncmp("lo", iface, 2)) {
                LOG(LogLevel::DEBUG, "Choosing ifname %s addr %s", iface, addr);

                SocketHelperUnix::setSocketBindingToIface(sfd, iface);
                iface_done = true;
            }
        });
    
    
    if (!iface_done) {
        LOG(LogLevel::ERROR, "Failed to find any valid interface to bind to (IPv4)");
        return ret;
    }
    SocketHelperCommon::printExternalIPINet();

    name.sin_family = AF_INET;
    name.sin_port = htons(kTgBotHostPort);
    name.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) !=
        0) {
        PLOG_E("Failed to bind to socket");
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

socket_handle_t SocketInterfaceUnixIPv4::createClientSocket() {
    socket_handle_t ret = kInvalidFD;
    struct sockaddr_in name {};
    const socket_handle_t sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG_E("Failed to create socket");
        return ret;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(SocketHelperCommon::getPortNumInet(this));
    inet_aton(getOptions(Options::DESTINATION_ADDRESS).c_str(), &name.sin_addr);
    if (connect(sfd, reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) !=
        0) {
        PLOG_E("Failed to connect to socket");
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

bool SocketInterfaceUnixIPv4::isAvailable() {
    return SocketHelperCommon::isAvailableIPv4(this);
}

void SocketInterfaceUnixIPv4::stopListening(const std::string& e) {
    forceStopListening();
}

void SocketInterfaceUnixIPv4::doGetRemoteAddr(socket_handle_t s) {
    SocketHelperUnix::doGetRemoteAddrInet<
        struct sockaddr_in, AF_INET, in_addr, INET_ADDRSTRLEN,
        offsetof(struct sockaddr_in, sin_addr)>(s);
}