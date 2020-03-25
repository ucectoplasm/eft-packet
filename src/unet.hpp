#pragma once

#include <winsock2.h>
#include <cstdint>
#include <vector>
#include "common.hpp"

// TODO (refactor): Move this into .cpp

namespace UNET
{
    enum SystemRequest { kConnect = 0x1, kConnectViaNetGroup = 0x2, kDisconnect = 0x3, kHeartbeat = 0x4, kBroadcastDiscovery = 0x9 };

#pragma pack(1)
    struct PacketBaseHeader
    {
        uint16_t connectionId;
    };

    struct NetPacketHeader : public PacketBaseHeader
    {
        uint16_t packetId;
        uint16_t sessionId;
    };

    struct NetMessageHeader
    {
        uint8_t  channelId;
        uint16_t len;
    };

    struct NetMessageReliableHeader
    {
        uint16_t messageId;
    };

    struct NetMessageOrderedHeader
    {
        uint8_t orderedMessageId;
    };

    struct NetMessageFragmentedHeader
    {
        uint8_t  fragmentedMessageId;
        uint8_t  fragmentIdx;
        uint8_t  fragmentAmnt;
    };

    struct PacketAcks128
    {
        uint16_t ackMessageId;
        uint32_t acks[4];
    };
#pragma pack()

    inline void decodePacketBaseHeader(PacketBaseHeader* hdr)
    {
        hdr->connectionId = ntohs(hdr->connectionId);
    }

    inline void decodeNetPacketHeader(NetPacketHeader* hdr)
    {
        hdr->packetId = ntohs(hdr->packetId);
    }

    inline void decode(NetMessageHeader* hdr)
    {
        hdr->len = ntohs(hdr->len);
    }

    inline void decode(NetMessageReliableHeader* hdr)
    {
        hdr->messageId = ntohs(hdr->messageId);
    }

    inline void decode(NetMessageFragmentedHeader* hdr) {}

    uint16_t decodeConnectionId(void* packet)
    {
        PacketBaseHeader* packet_hdr = reinterpret_cast<PacketBaseHeader*>(packet);
        decodePacketBaseHeader(packet_hdr);
        return packet_hdr->connectionId;
    }

    struct NetErrors
    {
        enum
        {
            kOk = 0x0,
            kWrongHost,
            kWrongConnection,
            kWrongChannel,
            kNoResources,
            kBadMessage,
            kTimeout,
            kMessageToLong,
            kWrongOperation,
            kVersionMismatch,
            kCRCMismatch,
            kDNSFailure,
            kUsageError,
            kLastError
        };
    };

    class AcksCache
    {
    public:
        AcksCache(const char* label)
        {
            m_acks.resize(65536);
            m_window_size = m_acks.size() / 2 - 1;
            m_head = m_window_size - 1;
            m_tail = 1;
            m_label = label;
            std::fill(std::begin(m_acks), std::end(m_acks), false);
        }

        bool ReadMessage(uint16_t message_id)
        {
            int max_distance = (int)m_acks.size();
            int raw_distance = abs(message_id - m_head);
            int distance = (raw_distance < (max_distance / 2)) ? raw_distance : max_distance - raw_distance;

            if (distance > m_window_size)
            {
                return false; // out of window
            }

            if (message_id < m_tail || message_id > m_head)
            {
                // this is suboptimal, could use a second queue of packets to reset instead
                for (int i = 0; i < distance; ++i)
                {
                    m_acks[m_tail] = false;
                    m_tail = (m_tail + 1) % m_acks.size();
                    m_head = (m_head + 1) % m_acks.size();
                }
            }

            bool acked = m_acks[message_id];
            if (!acked)
            {
                m_acks[message_id] = true;
            }
            return !acked;
        }

    private:
        std::vector<bool> m_acks;
        int m_head;
        int m_tail;
        int m_window_size;
        const char* m_label;
    };

    enum MesageDelimeters { kCombinedMessageDelimeter = 254, kReliableMessagesDelimeter = 255 };

    class MessageExtractorBase
    {
    public:
        MessageExtractorBase(char* data, uint16_t totalLength, uint8_t maxChannelId);
        char* GetMessageStart(void) const { return m_Data; }
        uint16_t GetMessageLength(void) const { return m_MessageLength; }
        uint8_t  GetError(void) const { return m_Error; }
        bool     IsError(void) const { return m_Error != (uint8_t)NetErrors::kOk; }
        uint16_t GetRemainingLength(void) const { return m_TotalLength; }
        uint8_t  GetChannelId(void) const { return m_ChannelId; }
        uint16_t GetFullMessageLength(void) const { return m_FullMessageLength; }
        bool     IsMessageCombined(void) const { return m_IsMessageCombined; }
    protected:
        bool     CheckIsChannelValid(void);
        bool     CheckIsLengthValid(void);
    protected:
        char* m_Data;
        uint16_t  m_TotalLength;
        uint16_t  m_MaxChannelId;
        uint8_t   m_Error;
        uint8_t   m_ChannelId;
        uint16_t  m_MessageLength;
        uint16_t  m_FullMessageLength;
        bool     m_IsMessageCombined;
    };

    class MessageExtractor : public MessageExtractorBase
    {
    public:
        MessageExtractor(char* data, uint16_t totalLength, uint8_t maxChannelId, AcksCache* acks);
        bool     GetNextMessage(void);
    private:
        bool     ExtractMessageHeader(void);
        bool     ExtractMessageLength(void);
        bool     ExtractMessage(void);

        AcksCache* m_Acks;
    };

    UNET::MessageExtractorBase::MessageExtractorBase(char* data, uint16_t totalLength, uint8_t maxChannelId)
        : m_Data(data)
        , m_TotalLength(totalLength)
        , m_MaxChannelId(maxChannelId)
        , m_Error((uint8_t)NetErrors::kOk)
        , m_ChannelId(0xFF)
        , m_MessageLength(0)
        , m_FullMessageLength(0)
        , m_IsMessageCombined(false)
    {
    }

    bool UNET::MessageExtractorBase::CheckIsChannelValid(void)
    {
        if (m_ChannelId > m_MaxChannelId)
        {
            m_Error = (uint8_t)NetErrors::kBadMessage;
            return false;
        }
        return true;
    }

    bool UNET::MessageExtractorBase::CheckIsLengthValid(void)
    {
        if (m_TotalLength < m_MessageLength)
        {
            m_Error = (uint8_t)NetErrors::kBadMessage;
            return false;
        }
        return true;
    }

    UNET::MessageExtractor::MessageExtractor(char* data, uint16_t totalLength, uint8_t maxChannelId, AcksCache* acks)
        : MessageExtractorBase(data, totalLength, maxChannelId),
        m_Acks(acks)
    {
    }


    bool UNET::MessageExtractor::GetNextMessage(void)
    {
        m_IsMessageCombined = false;
        m_Data += m_MessageLength;
        m_TotalLength -= m_MessageLength;
        m_FullMessageLength = 0;
        if (m_TotalLength == 0)
            return false;

        if (m_TotalLength < 2)
        {
            m_Error = (uint8_t)NetErrors::kBadMessage;
            return false;
        }

        m_ChannelId = *m_Data;
        if (m_ChannelId == (uint8_t)kReliableMessagesDelimeter)
        {
            ExtractMessageHeader();

            NetMessageReliableHeader* hr = reinterpret_cast<NetMessageReliableHeader*>(m_Data);
            decode(hr);
            if (!m_Acks->ReadMessage(hr->messageId))
            {
                return GetNextMessage();
            }
            m_Data += sizeof(NetMessageReliableHeader);
            m_TotalLength -= sizeof(NetMessageReliableHeader);
            m_MessageLength = 0;
            return GetNextMessage();
        }
        else if (m_ChannelId == kCombinedMessageDelimeter)
        {
            ++m_Data;
            --m_TotalLength;
            ++m_FullMessageLength;
            m_IsMessageCombined = true;
            return ExtractMessage();
        }
        if (!CheckIsChannelValid())
            return false;

        return ExtractMessage();
    }

    bool UNET::MessageExtractor::ExtractMessageHeader(void)
    {
        m_ChannelId = *m_Data;
        ++m_Data;
        --m_TotalLength;
        ++m_FullMessageLength;

        return ExtractMessageLength();
    }

    bool UNET::MessageExtractor::ExtractMessageLength(void)
    {
        uint8_t highByte = *m_Data;
        if (highByte & 0x80)
        {
            if (m_TotalLength < 2)
            {
                m_Error = (uint8_t)NetErrors::kBadMessage;
                return false;
            }
            m_MessageLength = ((highByte & 0x7F) << 8) + (uint8_t) * (m_Data + 1);
            m_TotalLength -= 2;
            m_Data += 2;
            m_FullMessageLength += 2;
        }
        else
        {
            m_MessageLength = highByte;
            --m_TotalLength;
            ++m_Data;
            ++m_FullMessageLength;
        }
        m_FullMessageLength += m_MessageLength;
        return true;
    }

    bool UNET::MessageExtractor::ExtractMessage(void)
    {
        if (m_TotalLength < 2)
        {
            m_Error = (uint8_t)NetErrors::kBadMessage;
            return false;
        }
        if (!ExtractMessageHeader())
            return false;

        if (!CheckIsLengthValid())
            return false;

        return true;
    }
}
