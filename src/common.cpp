#pragma once

#include "common.hpp"
#include "miniz/miniz.h"
#include "glm/gtx/euler_angles.hpp"

std::mutex g_world_lock;

std::vector<uint8_t> decompress_zlib(uint8_t* data, int len)
{
    std::vector<uint8_t> ret;
    ret.resize(len * 1);

    mz_stream_s stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = data;
    stream.avail_in = len;
    stream.next_out = ret.data();
    stream.avail_out = ret.size();
    mz_inflateInit(&stream);

    int err;

    while ((err = mz_inflate(&stream, MZ_NO_FLUSH)) == MZ_OK)
    {
        if (!stream.avail_out)
        {
            size_t len_old = ret.size();
            ret.resize(len_old * 2);
            stream.avail_out = len_old;
            stream.next_out = ret.data() + len_old;
        }
    }

    ret.resize(stream.total_out);
    return ret;
}

Vector3 to_euler(Quaternion& quat)
{
    glm::vec3 euler = glm::eulerAngles(glm::quat(quat.w, quat.x, quat.y, quat.z)) * 3.14159f / 180.f;
    return { euler.x, euler.y, euler.z };
}
