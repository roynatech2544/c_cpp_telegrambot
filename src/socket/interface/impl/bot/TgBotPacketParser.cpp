#include "TgBotPacketParser.hpp"

#include <absl/log/log.h>

#include <boost/crc.hpp>

#include "SharedMalloc.hpp"
#include "SocketBase.hpp"
#include "TgBotCommandExport.hpp"

using HandleState = TgBotSocketParser::HandleState;

HandleState TgBotSocketParser::handle_PacketHeader(
    std::optional<SharedMalloc>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    int64_t diff = 0;

    if (!socketData || socketData.value()->size != TgBotCommandPacket::hdr_sz) {
        LOG(ERROR) << "Failed to read from socket";
        return HandleState::Fail;
    }
    pkt = TgBotCommandPacket(TgBotCommandPacket::hdr_sz);
    socketData->assignTo(pkt->header);

    diff = pkt->header.magic - TgBotCommandPacketHeader::MAGIC_VALUE_BASE;
    if (diff != TgBotCommandPacketHeader::DATA_VERSION) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        if (diff >= 0) {
            LOG(INFO) << "This packet contains header data version " << diff
                      << ", but we have version "
                      << TgBotCommandPacketHeader::DATA_VERSION;
        }
        return HandleState::Ignore;
    }

    return HandleState::Ok;
}

HandleState TgBotSocketParser::handle_Packet(
    std::optional<SharedMalloc>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    boost::crc_32_type crc;

    if (TgBotCmd::isClientCommand(pkt->header.cmd)) {
        LOG(INFO) << "Received buf with " << TgBotCmd::toStr(pkt->header.cmd)
                  << ", invoke callback!";
    }

    if (!socketData.has_value()) {
        return HandleState::Ignore;
    }

    auto& socketDataVal = socketData.value();
    if (socketDataVal->size != pkt->header.data_size) {
        LOG(WARNING) << "Invalid packet data size, dropping buffer";
        return HandleState::Ignore;
    }

    crc.process_bytes(socketData->get(), pkt->header.data_size);
    if (crc.checksum() != pkt->header.checksum) {
        LOG(WARNING) << "Invalid packet checksum, dropping buffer";
        return HandleState::Ignore;
    }

    pkt->data->size = TgBotCommandPacket::hdr_sz + pkt->header.data_size;
    pkt->data->alloc();
    socketData->assignTo(pkt->data.get(), pkt->header.data_size);

    return HandleState::Ok;
}

bool TgBotSocketParser::onNewBuffer(SocketConnContext ctx) {
    std::optional<SharedMalloc> data;
    std::optional<TgBotCommandPacket> pkt;
    bool ret = false;

    data = interface->readFromSocket(ctx, sizeof(TgBotCommandPacketHeader));
    switch (handle_PacketHeader(data, pkt)) {
        case HandleState::Ok: {
            data = interface->readFromSocket(ctx, pkt->header.data_size);
            switch (handle_Packet(data, pkt)) {
                case HandleState::Ok: {
                    handle_CommandPacket(ctx, pkt.value());
                    break;
                }
                case HandleState::Ignore: {
                    break;
                }
                case HandleState::Fail: {
                    ret = true;
                    break;
                }
            }
            [[fallthrough]];
        }
        case HandleState::Ignore:
            ret = false;
            break;
        case HandleState::Fail:
            LOG(ERROR) << "Failed to handle packet header";
            ret = true;
            break;
    }
    return ret;
}