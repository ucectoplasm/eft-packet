#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

extern std::mutex g_world_lock;

std::vector<uint8_t> decompress_zlib(uint8_t* data, int len);

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

struct Vector3
{
    float x;
    float y;
    float z;
};

inline bool operator==(const Vector3 lhs, const Vector3 rhs)
{
    return abs(lhs.x - rhs.x) <= 0.0001f && abs(lhs.y - rhs.y) <= 0.0001f && abs(lhs.z - rhs.z) <= 0.0001f;
}

namespace std
{
    template<> struct hash<Vector3>
    {
        std::size_t operator()(Vector3 const& vec) const noexcept
        {
            std::size_t a = (size_t)vec.x;
            std::size_t b = (size_t)vec.y << 16;
            std::size_t c = (size_t)vec.z << 32;
            return a ^ b ^ c;
        }
    };
}

Vector3 to_euler(Quaternion& quat);
