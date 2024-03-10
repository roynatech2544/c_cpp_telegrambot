#include "SocketInterfaceBase.h"
#include <winsock2.h>

#define INTERNAL_SOCKET_PATH "C:\\Temp\\tgbot_internal.sock"
#define WSALOG_E(fmt, ...) LOG_E(fmt ": %s", ##__VA_ARGS__, strWSAError(WSAGetLastError()))

struct SocketInterfaceWindows : SocketInterfaceBase {
    using socket_handle_t = SOCKET;

    bool isValidSocketHandle(socket_handle_t handle)  {
        return handle != INVALID_SOCKET;
    };
    virtual socket_handle_t createClientSocket() = 0;
    virtual socket_handle_t createServerSocket() = 0;
    void writeToSocket(struct TgBotConnection conn) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t& cb, std::promise<bool>& createdPromise) override;
  
    virtual ~SocketInterfaceWindows() = default;
 protected:
    static char *strWSAError(const int errcode);
};

struct SocketInterfaceWindowsLocal : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    
  private:
    socket_handle_t makeSocket(bool is_client);
};