#pragma once

#include "common.hpp"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace tk
{
    class ByteStream;
    void process_packet(ByteStream* stream, uint8_t len, bool outbound);

    enum PacketCode : int16_t
    {
        ServerInit = 147,
        WorldSpawn = 151,
        WorldUnspawn = 152,
        SubworldSpawn = 153,
        SubworldUnspawn = 154,
        PlayerSpawn = 155,
        PlayerUnspawn = 156,
        ObserverSpawn = 157,
        ObserverUnspawn = 158,
        BattleEye = 168,
        GameUpdate = 170,
    };

    static uint32_t Popcount(uint32_t x)
    {
        uint32_t num = x - (x >> 1 & 1431655765U);
        uint32_t num2 = (num >> 2 & 858993459U) + (num & 858993459U);
        uint32_t num3 = (num2 >> 4) + num2 & 252645135U;
        uint32_t num4 = num3 + (num3 >> 8);
        return num4 + (num4 >> 16) & 63U;
    }

    static uint32_t Log2(uint32_t value)
    {
        uint32_t num = value | value >> 1;
        uint32_t num2 = num | num >> 2;
        uint32_t num3 = num2 | num2 >> 4;
        uint32_t num4 = num3 | num3 >> 8;
        return Popcount((num4 | num4 >> 16) >> 1);
    }

    static int BitRequired(int min, int max)
    {
        if (min != max)
        {
            return (int)(Log2((uint32_t)(max - min)) + 1);
        }
        return 0;
    }

    static void CalculateDataForQuantizing(float min, float max, float res, float* delta, int* maxIntegerValue, int* bits)
    {
        *delta = max - min;
        float f = *delta / res;
        *maxIntegerValue = (int)ceil(f);
        *bits = BitRequired(0, *maxIntegerValue);
    }

    static float DequantizeUIntToFloat(uint32_t integerValue, float min, int maxIntegerValue, float delta)
    {
        return integerValue / (float)maxIntegerValue * delta + min;
    }


    struct FloatQuantizer // copied from c# decompile
    {
        float Min;
        float Max;
        float Resolution;
        int BitsRequired;
        float Delta;
        int MaxIntegerValue;
        bool CheckBounds;

        FloatQuantizer(float min, float max, float resolution, bool checkBounds = false)
        {
            Min = min;
            Max = max;
            Resolution = resolution;
            CheckBounds = checkBounds;
            CalculateDataForQuantizing(Min, Max, Resolution, &Delta, &MaxIntegerValue, &BitsRequired);
        }

        float DequantizeUIntValue(uint32_t value)
        {
            return value / (float)MaxIntegerValue * Delta + Min;
        }
    };

    struct Vector2Quantizer // copied from c# decompile
    {
        FloatQuantizer _xFloatQuantizer;
        FloatQuantizer _yFloatQuantizer;

        Vector2Quantizer(float xMin, float xMax, float xResolution, float yMin, float yMax, float yResolution, bool checkBounds = false)
            : _xFloatQuantizer(xMin, xMax, xResolution, checkBounds),
            _yFloatQuantizer(yMin, yMax, yResolution, checkBounds)
        {
        }
    };

    struct Vector3Quantizer // copied from c# decompile
    {
        FloatQuantizer _xFloatQuantizer;
        FloatQuantizer _yFloatQuantizer;
        FloatQuantizer _zFloatQuantizer;

        Vector3Quantizer(float xMin, float xMax, float xResolution, float yMin, float yMax, float yResolution, float zMin, float zMax, float zResolution, bool checkBounds = false)
            : _xFloatQuantizer(xMin, xMax, xResolution, checkBounds),
            _yFloatQuantizer(yMin, yMax, yResolution, checkBounds),
            _zFloatQuantizer(zMin, zMax, zResolution, checkBounds)
        {
        }
    };

    class BitReader // copied from c# decompile (with tweaks)
    {
    public:
        BitReader(uint8_t* data, int len)
        {
            _data = (uint32_t*)data;
            _wordsCount = len / 4;
            _bitsCount = _wordsCount * 32;
            _bitsRead = 0;
            _bitIndex = 0;
            _wordIndex = 0;
            _isVirgin = true;
            _isOverflow = false;
        }

        uint32_t ReadBits(int bits)
        {
            if (_isVirgin)
            {
                _scratch = (uint64_t)(*_data);
                _isVirgin = false;
            }
            if (_bitsRead + bits > _bitsCount)
            {
                _isOverflow = true;
            }
            if (_isOverflow)
            {
                return 0;
            }
            _bitsRead += bits;
            if (_bitIndex + bits < 32)
            {
                _scratch <<= bits;
                _bitIndex += bits;
            }
            else
            {
                _wordIndex++;
                int num = 32 - _bitIndex;
                int num2 = bits - num;
                _scratch <<= num;
                _scratch |= (uint64_t)_data[_wordIndex];
                _scratch <<= num2;
                _bitIndex = num2;
            }
            uint32_t result = (uint32_t)(_scratch >> 32);
            _scratch &= uint32_t(~0u);
            return result;
        }

        std::vector<std::uint8_t> ReadBytes(int bytesCount)
        {
            std::vector<std::uint8_t> ret;
            ret.resize(bytesCount);

            if (_isVirgin)
            {
                _scratch = (uint64_t)(*_data);
                _isVirgin = false;
            }
            if (_bitsRead + bytesCount * 8 > _bitsCount)
            {
                std::memset(ret.data(), 0, bytesCount);
                _isOverflow = true;
                return ret;
            }
            int num = (4 - _bitIndex / 8) % 4;
            if (num > bytesCount)
            {
                num = bytesCount;
            }
            for (int i = 0; i < num; i++)
            {
                ret[i] = ReadBits(8);
            }
            if (num == bytesCount)
            {
                return ret;
            }
            int num2 = (bytesCount - num) / 4;
            if (num2 > 0)
            {
                int length = num2 * 4;
                int sourceIndex = _wordIndex * 4;
                memcpy(ret.data() + num, ((uint8_t*)_data) + sourceIndex, length);
                _bitsRead += num2 * 32;
                _wordIndex += num2;
                _scratch = (uint64_t)_data[_wordIndex];
            }
            int num3 = num + num2 * 4;
            int num4 = bytesCount - num3;
            for (int j = 0; j < num4; j++)
            {
                ret[num3 + j] = ReadBits(8);
            }
            return ret;
        }

        int GetAlignBits()
        {
            return (8 - _bitsRead % 8) % 8;
        }

        void ReadAlign()
        {
            int num = _bitsRead % 8;
            if (num != 0)
            {
                ReadBits(8 - num);
            }
        }

        void Reset()
        {
            _bitsRead = 0;
            _bitIndex = 0;
            _wordIndex = 0;
            _isVirgin = true;
            _isOverflow = false;
        }

        uint32_t* _data;
        int _wordsCount;
        int _bitsCount;
        int _bitsRead;
        uint64_t _scratch;
        int _bitIndex;
        int _wordIndex;
        bool _isOverflow;
        bool _isVirgin;
    };

    class BitStream // copied from c# decompile
    {
    public:

        BitStream(uint8_t* data, int len) : m_buffer(data, len)
        {
        }


        int ReadLimitedInt32(int min, int max)
        {
            int bits = BitRequired(min, max);
            int ret = m_buffer.ReadBits(bits) + (uint32_t)min;
            return ret;
        }

        bool ReadBool()
        {
            return m_buffer.ReadBits(1) > 0U;
        }

        float ReadFloat()
        {
            uint32_t ui = ReadUInt32();
            float flt;
            memcpy(&flt, &ui, 4);
            return flt;
        }

        uint8_t ReadUInt8()
        {
            uint8_t ret = m_buffer.ReadBits(8);
            return ret;
        }

        uint16_t ReadUInt16()
        {
            uint16_t ret = m_buffer.ReadBits(16);
            return ret;
        }

        uint32_t ReadUInt32()
        {
            uint32_t ret = m_buffer.ReadBits(32);
            return ret;
        }

        int32_t ReadInt32()
        {
            return (int32_t)ReadUInt32();
        }

        Vector3 ReadVector3()
        {
            return { ReadFloat(), ReadFloat(), ReadFloat() };
        }

        std::wstring ReadString(uint32_t max_size = 0)
        {
            if (ReadBool())
            {
                return {};
            }
            std::wstring data;
            m_buffer.ReadAlign();
            int num = ReadInt32();
            for (int i = 0; i < num; i++)
            {
                data.push_back(ReadChar());
            }
            return data;
        }

        std::wstring ReadLimitedString(wchar_t min, wchar_t max)
        {
            if (ReadBool())
            {
                return {};
            }

            std::wstring data;
            m_buffer.ReadAlign();
            int num = ReadInt32();
            int bits = BitRequired((int)min, (int)max);
            for (int i = 0; i < num; i++)
            {
                data.push_back(m_buffer.ReadBits(bits) + min);
            }
            return data;
        }

        wchar_t ReadChar()
        {
            return (wchar_t)m_buffer.ReadBits(16);
        }

        float ReadLimitedFloat(float min, float max, float res)
        {
            float delta;
            int maxIntegerValue;
            int bits;
            CalculateDataForQuantizing(min, max, res, &delta, &maxIntegerValue, &bits);

            float ret = DequantizeUIntToFloat(m_buffer.ReadBits(bits), min, maxIntegerValue, delta);
            return ret;
        }

        float ReadQuantizedFloat(FloatQuantizer* quantizer)
        {
            int required = quantizer->BitsRequired;
            float ret = quantizer->DequantizeUIntValue(m_buffer.ReadBits(required));
            return ret;
        }

        bool Overflow()
        {
            return false;
        }

        std::vector<std::uint8_t> ReadBytes(int len)
        {
            m_buffer.ReadAlign();
            return m_buffer.ReadBytes(len);
        }

        std::vector<std::uint8_t> ReadBytesAlloc()
        {
            uint16_t len = ReadUInt16();
            return ReadBytes(len);
        }

    private:

        BitReader m_buffer;
    };

    class ByteStream
    {
    public:
        ByteStream(uint8_t* data, int len) : m_data(data), m_len(len), m_head(0) { }

        bool ReadBool()
        {
            return ReadByte() == 1;
        }

        uint8_t ReadByte()
        {
            if (m_head < m_len)
            {
                uint8_t ret = 0;
                memcpy(&ret, m_data + m_head++, 1);
                return ret;
            }

            return 0;
        }

        uint16_t ReadUInt16()
        {
            uint16_t value = 0;
            value |= ReadByte();
            value |= (uint16_t)(ReadByte()) << 8;
            return value;
        }

        uint32_t ReadUInt32()
        {
            uint32_t value = 0;
            value |= ReadByte();
            value |= (uint32_t)(ReadByte()) << 8;
            value |= (uint32_t)(ReadByte()) << 16;
            value |= (uint32_t)(ReadByte()) << 24;
            return value;
        }

        int16_t ReadInt16()
        {
            uint16_t value = 0;
            value |= ReadByte();
            value |= (uint16_t)(ReadByte()) << 8;
            return (int16_t)value;
        }

        int32_t ReadInt32()
        {
            uint32_t value = 0;
            value |= ReadByte();
            value |= (uint32_t)(ReadByte()) << 8;
            value |= (uint32_t)(ReadByte()) << 16;
            value |= (uint32_t)(ReadByte()) << 24;
            return (int32_t)value;
        }

        int64_t ReadInt64()
        {
            uint64_t value = 0;

            uint64_t other = ReadByte();
            value |= other;

            other = ((uint64_t)ReadByte()) << 8;
            value |= other;

            other = ((uint64_t)ReadByte()) << 16;
            value |= other;

            other = ((uint64_t)ReadByte()) << 24;
            value |= other;

            other = ((uint64_t)ReadByte()) << 32;
            value |= other;

            other = ((uint64_t)ReadByte()) << 40;
            value |= other;

            other = ((uint64_t)ReadByte()) << 48;
            value |= other;

            other = ((uint64_t)ReadByte()) << 56;
            value |= other;

            return (int64_t)value;
        }

        float ReadSingle()
        {
            uint32_t val = ReadUInt32();
            float ret;
            memcpy(&ret, &val, 4);
            return ret;
        }

        std::vector<std::uint8_t> ReadBytes(int size)
        {
            std::vector<std::uint8_t> ret;
            ret.resize(size);

            for (int i = 0; i < size; ++i)
            {
                ret[i] = ReadByte();
            }

            return ret;
        }

        std::vector<std::uint8_t> ReadBytesAndSize()
        {
            uint16_t size = ReadUInt16();
            if (size == 0)
            {
                return {};
            }

            return ReadBytes(size);
        }

        Vector3 ReadVector3()
        {
            return { ReadSingle(), ReadSingle(), ReadSingle() };
        }

        Quaternion ReadQuaternion()
        {
            return { ReadSingle(), ReadSingle(), ReadSingle(), ReadSingle() };
        }

        int len() { return m_len; }
        void seek(int head) { m_head = head; }

    private:
        uint8_t* m_data;
        int m_len;
        int m_head;
    };
}
