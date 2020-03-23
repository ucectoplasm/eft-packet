#pragma once

#include <cstdint>
#include <vector>

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

Vector3 to_euler(Quaternion& quat);
