#include "pcap++/IPv4Layer.h"
#include "pcap++/IPv6Layer.h"
#include "pcap++/Packet.h"
#include "pcap++/PcapLiveDevice.h"
#include "pcap++/PcapLiveDeviceList.h"
#include "pcap++/UdpLayer.h"

#include "gl/glew.h"
#include "gl/glu.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "SDL/SDL.h"
#include "SDL/SDL_opengl.h"
#include "unet.hpp"
#include "common.hpp"

#include "tk.hpp"
#include "tk_net.hpp"

#include <mutex>
#include <memory>
#include <thread>
#include <unordered_map>

#define GLT_IMPLEMENTATION
#include "gltext.h"

#define LOCAL_ADAPTER_IP_ADDRESS "fill.me.in" // ipconfig in cmd prompt on cheat machine, find local address, fill it in here
#define MACHINE_PLAYING_GAME_IP_ADDRESS "fill.me.in" // the local IP address of the machine communicating with EFT servers

struct Packet
{
    int timestamp;
    bool outbound;
    std::vector<uint8_t> data;
    std::string src_ip;
    std::string dst_ip;
};

struct WorkGroup
{
    std::mutex work_guard;
    std::vector<Packet> work;
};

struct GraphicsState
{
    SDL_GLContext ctx;
    SDL_Window* window;

    GLuint shader;
    GLuint vao;
    GLuint vbo;
    GLuint line_vao;
    GLuint line_vbo;

    int width;
    int height;
};

void do_net(std::vector<Packet> work, const char* packet_dump_path);
void do_render(GraphicsState* state);

GraphicsState make_gfx(SDL_GLContext ctx, SDL_Window* window);
void resize_gfx(GraphicsState* state, int width, int height);

int SDL_main(int argc, char* argv[])
{
    const char* packet_dump_path = argc >= 2 ? argv[1] : nullptr;
    bool dump_packets = argc >= 3 && argv[2][0] == '1';

    static int s_base_time = GetTickCount();
    static float time_scale = argc >= 4 ? atof(argv[3]) : 1.0f;

    std::unique_ptr<std::thread> processing_thread;
    pcpp::PcapLiveDevice* dev = nullptr;

    WorkGroup work;

    if (packet_dump_path && !dump_packets)
    {
        // We load packets for offline replay on another thread.
        processing_thread = std::make_unique<std::thread>(
            [packet_dump_path, &work]()
            {
                FILE* file = fopen(packet_dump_path, "rb");

                bool outbound;
                while (fread(&outbound, sizeof(outbound), 1, file) == 1)
                {
                    int timestamp;
                    fread(&timestamp, 4, 1, file);

                    int size;
                    fread(&size, 4, 1, file);

                    std::vector<uint8_t> packet;
                    packet.resize(size);
                    fread(packet.data(), size, 1, file);

                    static int s_first_packet_time = timestamp;

                    while ((GetTickCount() - s_base_time) * time_scale < (uint32_t)timestamp - s_first_packet_time)
                    {
                        std::this_thread::yield(); // sleep might be better? doesn't matter much
                    }

                    std::lock_guard<std::mutex> lock(work.work_guard);
                    work.work.push_back({ timestamp, outbound, std::move(packet) });
                }

                fclose(file);
            });
    }
    else
    {
        dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIp(LOCAL_ADAPTER_IP_ADDRESS);
        dev->open();

        pcpp::ProtoFilter filter(pcpp::UDP);
        dev->setFilter(filter);

        static std::string s_server_ip;

        dev->startCapture(
            [](pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* user_data)
            {
                pcpp::Packet parsed(packet);

                std::string src_ip;
                std::string dst_ip;

                if (pcpp::IPv4Layer* ip = parsed.getLayerOfType<pcpp::IPv4Layer>())
                {
                    src_ip = ip->getSrcIpAddress().toString();
                    dst_ip = ip->getDstIpAddress().toString();
                }
                else if (pcpp::IPv6Layer* ip = parsed.getLayerOfType<pcpp::IPv6Layer>())
                {
                    src_ip = ip->getSrcIpAddress().toString();
                    dst_ip = ip->getDstIpAddress().toString();
                }

                pcpp::UdpLayer* udp = parsed.getLayerOfType<pcpp::UdpLayer>();
                if (udp == NULL) {
                    return;
                }

                int len = udp->getDataLen() - udp->getHeaderLen();
                uint8_t* data_start = udp->getDataPtr(udp->getHeaderLen());

                int timestamp = (int)(GetTickCount() - s_base_time);
                bool outbound = src_ip == MACHINE_PLAYING_GAME_IP_ADDRESS;
                std::vector<uint8_t> data;
                data.resize(len);
                memcpy(data.data(), data_start, len);

                WorkGroup& work = *(WorkGroup*)user_data;
                std::lock_guard<std::mutex> lock(work.work_guard);
                work.work.push_back({ timestamp , outbound, std::move(data), std::move(src_ip), std::move(dst_ip) });
            }, &work);
    }

    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

    SDL_Window* win = SDL_CreateWindow("Bastian Suter's Queef II: UC Strikes Back", 100, 100, 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    glewInit();
    SDL_GL_SetSwapInterval(1);

    GraphicsState gfx = make_gfx(ctx, win);

    bool quit = false;

    std::unique_ptr<std::thread> net_thread = std::make_unique<std::thread>(
        [&]()
        {
            while (!quit)
            {
                std::vector<Packet> local_work;

                {
                    std::lock_guard<std::mutex> lock(work.work_guard);
                    std::swap(local_work, work.work);
                }

                if (!local_work.empty())
                {
                    do_net(std::move(local_work), dump_packets ? packet_dump_path : nullptr);
                }
                else
                {
                    SDL_Delay(10);
                }
            }
        });

    while (!quit)
    {
        SDL_Event e;
        if (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    resize_gfx(&gfx, e.window.data1, e.window.data2);
                }
            }
        }

        do_render(&gfx);
    }

    net_thread->join();
    gltTerminate();

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);

    if (processing_thread)
    {
        processing_thread->join();
    }

    if (dev)
    {
        dev->stopCapture();
        dev->close();
    }

    return 0;
}

struct FragmentedMessage
{
    int parts;
    std::vector<std::unique_ptr<std::vector<uint8_t>>> packets;
};

void do_net(std::vector<Packet> work, const char* packet_dump_path)
{
    auto write_packet = [packet_dump_path](Packet& packet)
    {
        if (packet_dump_path)
        {
            static FILE* s_file = fopen(packet_dump_path, "ab"); // handle technically leaks
            fwrite(&packet.outbound, sizeof(packet.outbound), 1, s_file);
            fwrite(&packet.timestamp, 4, 1, s_file);
            int len = (int)packet.data.size();
            fwrite(&len, 4, 1, s_file);
            fwrite(packet.data.data(), len, 1, s_file);
        }
    };

    static std::unique_ptr<UNET::AcksCache> s_inbound_acks;
    static std::unique_ptr<UNET::AcksCache> s_outbound_acks;
    static std::unordered_map<int, FragmentedMessage> s_messages;

    for (Packet& packet : work)
    {
        if (packet.data.size() <= 3)
        {
            // Too short for us to care about. Proceed.
            continue;
        }

        uint8_t* data_start = packet.data.data();
        int data_len = (int)packet.data.size();

        if (uint16_t conn_id = UNET::decodeConnectionId(data_start); !conn_id)
        {
            if (data_start[2] == UNET::kConnect) // This is a connection attempt. Let's establish a state.
            {
                s_inbound_acks = std::make_unique<UNET::AcksCache>("INBOUND");
                s_outbound_acks = std::make_unique<UNET::AcksCache>("OUTBOUND");
                s_messages.clear();

                std::lock_guard<std::mutex> lock(g_world_lock);
                tk::g_state = std::make_unique<tk::GlobalState>();
                tk::g_state->server_ip = packet.dst_ip.empty() ? "LOCAL_REPLAY" : packet.dst_ip;
                tk::g_state->loot_db = std::make_unique<tk::LootDatabase>();
            }

            write_packet(packet);
            continue;
        }

        bool no_state = tk::g_state == nullptr;
        bool no_server = no_state || tk::g_state->server_ip.empty();
        bool filtered_out = no_server ||
            (   tk::g_state->server_ip != packet.src_ip &&
                tk::g_state->server_ip != packet.dst_ip &&
                tk::g_state->server_ip != "LOCAL_REPLAY");

        if (filtered_out)
        {
            continue;
        }

        write_packet(packet);
        // Now we strip the UNET-isms...

        UNET::NetPacketHeader* packet_hdr = reinterpret_cast<UNET::NetPacketHeader*>(data_start);
        decodeNetPacketHeader(packet_hdr);
        data_start += sizeof(UNET::NetPacketHeader);
        data_len -= sizeof(UNET::NetPacketHeader);

        UNET::PacketAcks128* acks = reinterpret_cast<UNET::PacketAcks128*>(data_start);
        // probably need to decode??
        data_start += sizeof(UNET::PacketAcks128);
        data_len -= sizeof(UNET::PacketAcks128);

        UNET::AcksCache* received_acks = packet.outbound ? s_outbound_acks.get() : s_inbound_acks.get();

        UNET::MessageExtractor messageExtractor((char*)data_start, data_len, 3 + (102*2), received_acks);
        while (messageExtractor.GetNextMessage())
        {
            std::unique_ptr<std::vector<uint8_t>> complete_message;

            {
                uint8_t* user_data = (uint8_t*)messageExtractor.GetMessageStart();
                int user_len = messageExtractor.GetMessageLength();
                int channel = messageExtractor.GetChannelId();

                if (user_len <= sizeof(UNET::NetMessageFragmentedHeader)) {
                    continue;
                }

                if (channel == 0 || channel == 1 || channel == 2) // ReliableFragmented
                {
                    UNET::NetMessageFragmentedHeader* hr = reinterpret_cast<UNET::NetMessageFragmentedHeader*>(user_data);
                    UNET::decode(hr);
                    user_data += sizeof(UNET::NetMessageFragmentedHeader);
                    user_len -= sizeof(UNET::NetMessageFragmentedHeader);

                    std::unique_ptr<std::vector<std::uint8_t>> data = std::make_unique<std::vector<std::uint8_t>>();
                    data->resize(user_len);
                    memcpy(data->data(), user_data, user_len);
                    int key = hr->fragmentedMessageId | channel << 8;

                    s_messages[key].parts = hr->fragmentAmnt;
                    s_messages[key].packets.resize(hr->fragmentAmnt);

                    if (hr->fragmentIdx >= s_messages[key].packets.size())
                    {
                        // broken fragment
                    }
                    else
                    {
                        s_messages[key].packets[hr->fragmentIdx] = std::move(data);
                    }

                    bool complete = true;

                    for (int i = 0; i < hr->fragmentAmnt; ++i)
                    {
                        if (!s_messages[key].packets[i])
                        {
                            complete = false;
                            break;
                        }
                    }

                    if (complete)
                    {
                        for (int i = 1; i < hr->fragmentAmnt; ++i)
                        {
                            s_messages[key].packets[0]->insert(
                                std::end(*s_messages[key].packets[0]),
                                std::begin(*s_messages[key].packets[i]),
                                std::end(*s_messages[key].packets[i]));
                        }

                        complete_message = std::move(s_messages[key].packets[0]);
                        s_messages.erase(key);
                    }
                }
                else
                {
                    if (channel % 2 == 1)
                    {
                        UNET::NetMessageReliableHeader* hr = reinterpret_cast<UNET::NetMessageReliableHeader*>(user_data);
                        decode(hr);
                        if (!received_acks->ReadMessage(hr->messageId))
                        {
                            continue;
                        }
                    }

                    // channel % 2 == 0 does not have NetMessageReliableHeader but skip same bytes so this works
                    user_data += sizeof(UNET::NetMessageReliableHeader);
                    user_data += sizeof(UNET::NetMessageOrderedHeader);
                    user_len -= sizeof(UNET::NetMessageReliableHeader);
                    user_len -= sizeof(UNET::NetMessageOrderedHeader);
                    complete_message = std::make_unique<std::vector<uint8_t>>();
                    complete_message->resize(user_len);
                    memcpy(complete_message->data(), user_data, user_len);
                }
            }

            if (complete_message)
            {
                tk::ByteStream str(complete_message->data(), (int)complete_message->size());
                tk::process_packet(&str, messageExtractor.GetChannelId(), packet.outbound);
            }
        }
    }
}

bool get_loot_information(tk::LootEntry* entry, bool include_equipment, bool* draw_text, bool* draw_beam, float* beam_height, int* r, int* g, int* b)
{
    // put your loot highlighting logic here
    bool draw = true;
    *r = 0;
    *g = 0;
    *b = 0;
    *draw_beam = false;
    *draw_text = !entry->unknown;

    // highlight overrides all
    if (entry->highlighted)
    {
        draw = true;
        *draw_beam = true;
        *beam_height = 200.0f;
        *draw_text = true;
        *r = 153;
        *g = 101;
        *b = 21;
    }

    return draw;
}

void do_render(GraphicsState* gfx)
{
    // ye who read this code, judge its performance (and lack of state caching) not
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        std::lock_guard<std::mutex> lock(g_world_lock); // would be more efficient if we locked, gathered data, then unlocked, then rendered

        if (tk::g_state && tk::g_state->map)
        {
            glm::mat4 projection = glm::perspective(glm::radians(75.0f), (float)gfx->width / (float)gfx->height, 0.1f, 2000.0f);

            // flip x axis, from right handed (gl) to left handed (unity)
            projection = glm::scale(projection, glm::vec3(-1.0f, 1.0f, 1.0f));

            auto get_forward_vec = [](float pitch, float yaw, glm::vec3 pos)
            {
                float elevation = glm::radians(-pitch);
                float heading = glm::radians(yaw);
                glm::vec3 forward_vec(cos(elevation) * sin(heading), sin(elevation), cos(elevation) * cos(heading));
                return forward_vec;
            };

            auto get_alpha_for_y = [](float y1, float y2)
            {
                return abs(y1 - y2) >= 3.0f ? 63 : 255;
            };

            float player_y = 0.0f;
            glm::vec3 player_forward_vec;

            if (tk::Observer* player = tk::g_state->map->get_player(); player)
            {
                player_y = player->pos.y;
                float pitch = player->rot.y;
                float yaw = player->rot.x;
                glm::vec3 cam_at(player->pos.x, player->pos.y + 1.5f, player->pos.z);
                player_forward_vec = get_forward_vec(pitch, yaw, cam_at);
                glm::vec3 cam_look = cam_at + player_forward_vec;
                glm::mat4 view = glm::lookAt(cam_at, cam_look, { 0.0f, 1.0f, 0.0f });

                glUseProgram(gfx->shader);
                glUniform1f(glGetUniformLocation(gfx->shader, "player_y"), player_y);
                glUniformMatrix4fv(glGetUniformLocation(gfx->shader, "projection"), 1, GL_FALSE, &projection[0][0]);
                glUniformMatrix4fv(glGetUniformLocation(gfx->shader, "view"), 1, GL_FALSE, &view[0][0]);

                auto draw_text = [&gfx]
                    (float x, float y, float z, float scale, const char* txt, int r, int g, int b, int a, glm::mat4* view, glm::mat4* proj)
                {
                    GLTtext* text = gltCreateText();
                    gltSetText(text, txt);
                    gltBeginDraw();
                    gltColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
                    gltDrawText3D(text, x, y, z, scale, (GLfloat*)&view[0][0], (GLfloat*)&proj[0][0]);
                    gltEndDraw();
                    gltDeleteText(text);
                };

                auto draw_box = [&gfx]
                    (float x, float y, float z, float scale_x, float scale_y, float scale_z, int r, int g, int b)
                {
                    glm::mat4 model = glm::mat4(1.0f);
                    glm::vec3 pos(x, y, z);
                    model = glm::translate(model, pos) * glm::scale(model, glm::vec3(scale_x, scale_y, scale_z));
                    glUseProgram(gfx->shader);
                    glUniform1i(glGetUniformLocation(gfx->shader, "line"), 0);
                    glUniform1f(glGetUniformLocation(gfx->shader, "obj_y"), y - (scale_y / 2.0f));
                    glUniform3f(glGetUniformLocation(gfx->shader, "color"), r / 255.0f, g / 255.0f, b / 255.0f);
                    glUniformMatrix4fv(glGetUniformLocation(gfx->shader, "model"), 1, GL_FALSE, &model[0][0]);
                    glBindVertexArray(gfx->vao);
                    glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                };

                auto draw_line = [&gfx]
                    (float x, float y, float z, float to_x, float to_y, float to_z, int r, int g, int b, int a)
                {
                    float vertices[] =
                    {
                        x, y, z,
                        to_x, to_y, to_z,
                    };

                    glUseProgram(gfx->shader);
                    glUniform1i(glGetUniformLocation(gfx->shader, "line"), a);
                    glUniform3f(glGetUniformLocation(gfx->shader, "color"), r / 255.0f, g / 255.0f, b / 255.0f);
                    glBindBuffer(GL_ARRAY_BUFFER, gfx->line_vbo);
                    glBindVertexArray(gfx->line_vao);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
                    glDrawArrays(GL_LINES, 0, 2);
                };

                static std::unordered_map<std::string, std::tuple<uint8_t, uint8_t, uint8_t>> s_group_map; // maybe reset on map change?

                for (tk::Observer* obs : tk::g_state->map->get_observers())
                {
                    if (obs->type == tk::Observer::Self)
                    {
                        glm::vec3 at(obs->pos.x, obs->pos.y, obs->pos.z);
                        glm::vec3 look = at + (get_forward_vec(obs->rot.y, obs->rot.x, at) * 50.0f);
                        look.y += 1.5f;
                        draw_line(at.x, at.y, at.z, look.x, look.y, look.z, 0, 255, 0, 255);
                        continue;
                    }

                    uint8_t r = 0;
                    uint8_t g = 0;
                    uint8_t b = 0;

                    float scale_x = 1.0f;
                    float scale_y = 2.0f;
                    float scale_z = 1.0f;

                    if (obs->is_unspawned)
                    {
                        r = 179;
                        g = 120;
                        b = 211;
                    }
                    else
                    {
                        if (obs->is_dead)
                        {
                            scale_x = 2.0f;
                            scale_y = 1.0f;
                        }

                        if (obs->type == tk::Observer::Scav && obs->is_npc)
                        {
                            r = 255;
                            g = obs->is_dead ? 140 : 255;
                        }
                        else
                        {
                            r = obs->is_dead ? 139 : 255;
                        }
                    }

                    if (!obs->group_id.empty())
                    {
                        if (obs->group_id == player->group_id)
                        {
                            r = 0;
                            g = 255;
                            b = 0;
                        }
                        else if (auto entry = s_group_map.find(obs->group_id); entry == std::end(s_group_map))
                        {
                            s_group_map[obs->group_id] = std::make_tuple(rand() % 256, rand() % 256, rand() % 256);
                        }
                    }

                    if (!obs->is_dead && !obs->is_unspawned)
                    {
                        glm::vec3 at(obs->pos.x, obs->pos.y, obs->pos.z);
                        glm::vec3 enemy_forward_vec = get_forward_vec(obs->rot.y, obs->rot.x, at);
                        bool facing_towards_player = glm::dot(player_forward_vec, enemy_forward_vec) < -0.0f;
                        int alpha = facing_towards_player ? 255 : 63;
                        glm::vec3 look = at + (enemy_forward_vec * (facing_towards_player ? 75.0f : 12.5f));
                        draw_line(at.x, at.y, at.z, look.x, look.y, look.z, r, g, b, alpha);
                    }

                    draw_box(obs->pos.x, obs->pos.y, obs->pos.z, scale_x, scale_y, scale_z, r, g, b);
                }

                for (Vector3* pos : tk::g_state->map->get_static_corpses())
                {
                    draw_box(pos->x, pos->y, pos->z, 2.0f, 1.0f, 1.0f, 102, 0, 102);
                }

                std::vector<std::tuple<Vector3, std::string, int, int, int>> loot_text_to_render;

                auto draw_loot = [&](tk::LootEntry* entry, bool include_equipment = true)
                {
                    bool draw_text, draw_beam;
                    float beam_height;
                    int r, g, b;
                    bool draw = get_loot_information(entry, include_equipment, &draw_text, &draw_beam, &beam_height, &r, &g, &b);

                    if (draw)
                    {
                        glm::vec3 player_pos(player->pos.x, player->pos.y, player->pos.z);
                        glm::vec3 entry_pos(entry->pos.x, entry->pos.y, entry->pos.z);

                        static constexpr float MIN_DISTANCE_FOR_LOOT = 10.0f;
                        if (float distance = glm::length(entry_pos - player_pos); distance >= MIN_DISTANCE_FOR_LOOT)
                        {
                            if (draw_text)
                            {
                                loot_text_to_render.push_back(std::make_tuple(entry->pos, entry->name, r, g, b));
                            }

                            if (draw_beam)
                            {
                                draw_box(entry->pos.x, entry->pos.y + beam_height / 2.0f, entry->pos.z, 0.3f, beam_height, 0.3f, r, g, b);
                            }
                        }

                        draw_box(entry->pos.x, entry->pos.y, entry->pos.z, 0.3f, 0.3f, 0.3f, r, g, b);
                    }
                };

                for (tk::LootEntry* entry : tk::g_state->map->get_loot())
                {
                    draw_loot(entry);
                }

                for (tk::TemporaryLoot* entry : tk::g_state->map->get_temporary_loots())
                {
                    draw_box(entry->pos.x, entry->pos.y + 1.5f, entry->pos.z, 0.15f, 3.0f, 0.15f, 0, 200, 200);
                    draw_box(entry->pos.x, entry->pos.y, entry->pos.z, 0.25f, 0.25f, 0.25f, 0, 200, 200);
                }

                for (tk::Observer* obs : tk::g_state->map->get_observers())
                {
                    if (obs->type == tk::Observer::ObserverType::Self)
                    {
                        continue;
                    }

                    int r = 255;
                    int g = 255;
                    int b = 255;

                    if (auto entry = s_group_map.find(obs->group_id); entry != std::end(s_group_map))
                    {
                        r = std::get<0>(entry->second);
                        g = std::get<1>(entry->second);
                        b = std::get<2>(entry->second);
                    }

                    glm::vec3 player_pos(player->pos.x, player->pos.y, player->pos.z);
                    glm::vec3 obs_pos(obs->pos.x, obs->pos.y, obs->pos.z);

                    static constexpr float MIN_DISTANCE_FOR_DISTANCE = 30.0f;
                    if (float distance = glm::length(obs_pos - player_pos); distance >= MIN_DISTANCE_FOR_DISTANCE)
                    {
                        draw_text(obs->pos.x, obs->pos.y + 3.0f, obs->pos.z, 0.25f, std::to_string((int)distance).c_str(), r, g, b, get_alpha_for_y(player_y, obs->pos.y), &view, &projection);
                    }

                    int total_val = 0;

                    for (tk::LootEntry& entry : obs->inventory)
                    {
                        entry.pos = obs->pos;
                        entry.pos.y += 0.5f;
                        draw_loot(&entry, false);
                        total_val += entry.value;
                    }

                    std::string val(16, '\0');
                    val.resize(std::snprintf(val.data(), val.size(), "%.1f", total_val / 1000.0f));
                    std::string name_and_val = obs->name + " (" + val + "k)";
                    draw_text(obs->pos.x, obs->pos.y + 2.0f, obs->pos.z, 0.05f, name_and_val.c_str(), r, g, b, get_alpha_for_y(player_y, obs->pos.y), &view, &projection);
                }

                for (auto& [pos, txt, r, g, b] : loot_text_to_render)
                {
                    draw_text(pos.x, pos.y + 0.5f, pos.z, 0.05f, txt.c_str(), r, g, b, get_alpha_for_y(player_y, pos.y), &view, &projection);
                }

                // TODO: Render
                // Sort text by distance from camera and render in order to fix transparency overlap.
            }
        }
    }

    SDL_GL_SwapWindow(gfx->window);
    SDL_Delay(32);
}

GraphicsState make_gfx(SDL_GLContext ctx, SDL_Window* window)
{
    gltInit();

    GraphicsState gfx;
    gfx.ctx = ctx;
    gfx.window = window;
    gfx.shader = glCreateProgram();

    auto make_shader = [](GLuint type, const char* shader) -> GLuint
    {
        GLuint handle = glCreateShader(type);
        glShaderSource(handle, 1, &shader, NULL);
        glCompileShader(handle);

        int success;
        glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char info[512];
            glGetShaderInfoLog(handle, 512, NULL, info);
            printf("Failed to compile shader: %s\n", info);
        };

        return handle;
    };

    GLuint vtx_shader = make_shader(GL_VERTEX_SHADER,
        R"(
            #version 330 core

            layout (location = 0) in vec3 pos;
            layout (location = 1) in vec2 tex_coord;

            out float _alpha;

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;
            uniform int line;
            uniform float player_y;
            uniform float obj_y;

            void main()
            {
                if (line > 0)
                {
                    gl_Position = projection * view * vec4(pos, 1.0f);
                    _alpha = line / 255.0f;
                }
                else
                {
                    gl_Position = projection * view * model * vec4(pos, 1.0f);
                    _alpha = abs(player_y - obj_y) >= 3.0f ? 0.25f : 1.0f;
                }
            }
        )"
    );

    GLuint pixel_shader = make_shader(GL_FRAGMENT_SHADER,
        R"(
            #version 330 core

            out vec4 FragColor;
            in float _alpha;
            uniform vec3 color;

            void main()
            {
                FragColor = vec4(color, _alpha);
            }
        )"
    );

    glAttachShader(gfx.shader, vtx_shader);
    glAttachShader(gfx.shader, pixel_shader);
    glLinkProgram(gfx.shader);

    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    glGenVertexArrays(1, &gfx.vao);
    glGenBuffers(1, &gfx.vbo);
    glBindVertexArray(gfx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &gfx.line_vao);
    glGenBuffers(1, &gfx.line_vbo);
    glBindVertexArray(gfx.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.line_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int width;
    int height;
    SDL_GetWindowSize(gfx.window, &width, &height);
    resize_gfx(&gfx, width, height);

    return gfx;
}

void resize_gfx(GraphicsState* state, int width, int height)
{
    glViewport(0, 0, width, height);
    state->width = width;
    state->height = height;
}
