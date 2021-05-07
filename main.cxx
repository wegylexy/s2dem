#include "config.h"
#include "tile.h"
#include <atomic>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <GeographicLib/Geoid.hpp>
#include <s2/s2cell.h>
#include <s2/s2latlng.h>

#ifdef WINDOWS
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

using namespace std;
using namespace std::filesystem;
using namespace GeographicLib;

static volatile sig_atomic_t cancelled = 0;

int main(int argc, const char *argv[])
{
    signal(SIGINT, [](int _) { cancelled = 1; });
    Geoid geoid{"egm96-5"};
    geoid.CacheAll();
    string mode{argv[1]};
    if (mode == "g")
    {
        ofstream s;
        s.open(string{argv[2]}.append("/egm96-s").append(argv[3]).append(".s2g"), ofstream::trunc);
        const auto level = stoi(argv[3]);
        const auto b = S2CellId::FromFace(0).child_begin(level), e = S2CellId::FromFace(5).child_end(level);
        const float offset = geoid.Offset(), scale = geoid.Scale();
        s.write(reinterpret_cast<const char *>(&offset), sizeof(float));
        s.write(reinterpret_cast<const char *>(&scale), sizeof(float));
        {
            const auto b_id = b.id(), e_id = e.id();
            s.write(reinterpret_cast<const char *>(&b_id), sizeof(b_id));
            s.write(reinterpret_cast<const char *>(&e_id), sizeof(e_id));
        }
        for (auto c = b; c != e; c = c.next())
        {
            const auto ll = c.ToLatLng();
            const auto v = static_cast<uint16_t>(roundeven(geoid.ConvertHeight(ll.lat().degrees(), ll.lng().degrees(), 0, Geoid::convertflag::GEOIDTOELLIPSOID) / scale - offset));
            s.write(reinterpret_cast<const char *>(&v), sizeof(v));
        }
        s.flush();
        s.close();
    }
    else if (mode == "edem")
    {
        shared_ptr<Tile<>> tiles[24];
        for (char g = 'A'; g <= 'X'; ++g)
        {
            ifstream s;
            s.open(string{argv[2]}.append("/15-").append(1, g).append(".tif"));
            tiles[g - 'A'] = Tile<>::Load(s);
            s.close();
        }
        const auto group_level = stoi(argv[4]), level = stoi(argv[5]);
        const auto length = 1 << (level - group_level << 1); // 4 ** (level - group_level)
        const auto gb = S2CellId::FromFace(0).child_begin(group_level), ge = S2CellId::FromFace(5).child_end(group_level);
        atomic_flag g_lock{};
        auto g = gb.prev();
        const auto hc = thread::hardware_concurrency();
        vector<thread> threads;
        threads.reserve(hc);
        for (int i{}; i < hc; ++i)
        {
            threads.push_back(thread{[argv, &geoid, tiles, level, length, &g_lock, &g, ge]() {
                while (!cancelled)
                {
                    while (g_lock.test_and_set())
                        continue;
                    const auto current = g.next();
                    if (current == ge)
                    {
                        g_lock.clear();
                        return;
                    }
                    g = current;
                    g_lock.clear();
                    float min_value{}, max_value{}, *values = new float[length];
                    int count{}, i{};
                    const auto b = current.child_begin(level), e = current.child_end(level);
                    for (auto c = b; c != e; c = c.next(), ++i)
                    {
                        const auto ll = c.ToLatLng();
                        const auto lat = ll.lat().degrees(), lon = ll.lng().degrees();
                        const auto grid = static_cast<int>(90 - lat) / 45 * 6 + static_cast<int>(lon + 180) / 60;
                        const auto value = values[i] = geoid.ConvertHeight(lat, lon, tiles[grid]->Interpolate(lat, lon), Geoid::convertflag::GEOIDTOELLIPSOID);
                        if (value < min_value)
                            min_value = value;
                        if (value > max_value)
                            max_value = value;
                        if (value)
                            ++count;
                    }
                    if (count)
                    {
                        ofstream s;
                        s.open(string{argv[3]} + "/e" + g.ToToken() + ".s2dem", ofstream::trunc);
                        const float offset = min_value, scale = ceil(max_value - min_value) / 65535;
                        s.write(reinterpret_cast<const char *>(&offset), sizeof(float));
                        s.write(reinterpret_cast<const char *>(&scale), sizeof(float));
                        {
                            const auto b_id = b.id(), e_id = e.id();
                            s.write(reinterpret_cast<const char *>(&b_id), sizeof(b_id));
                            s.write(reinterpret_cast<const char *>(&e_id), sizeof(e_id));
                        }
                        for (int i{}; i < length; ++i)
                        {
                            const auto v = static_cast<uint16_t>(roundeven(values[i] / scale - offset));
                            s.write(reinterpret_cast<const char *>(&v), sizeof(v));
                        }
                        s.flush();
                        s.close();
                    }
                    delete[] values;
                }
            }});
        }
        for (auto &t : threads)
            t.join();
    }
    else
    {
        cerr << "Usage: s2dem g ~/geoids {level}" << endl;
        cerr << "Usage: s2dem edem ~/dem/15 ~/dem/s2 {group-level} {level}" << endl;
        return 1;
    }
    return 0;
}