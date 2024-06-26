#pragma once

#include <CStringLifetime.h>
#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>

#include <ManagedThreads.hpp>
#include <future>
#include <initcalls/Initcall.hpp>
#include <memory>
#include <mutex>

#include "SocketBase.hpp"

struct NetworkLogSink : absl::LogSink, InitCall, ManagedThreadRunnable {
    void Send(const absl::LogEntry& entry) override;

    void doInitCall() override;

    void runFunction() override;

    const CStringLifetime getInitCallName() const override;

    explicit NetworkLogSink();

    std::shared_ptr<SocketInterfaceBase> interface;
    bool enabled = true;
    std::promise<void> onClientDisconnected;

    std::mutex mContextMutex;  // Protects context
    SocketConnContext* context = nullptr;
};

