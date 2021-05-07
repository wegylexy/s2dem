#include "tile.h"
#include <cmath>
#include <functional>
#include <tiffio.hxx>
#include <xtiffio.h>

using namespace std;

static size_t size0{};

inline double RoundD(double d)
{
    return (static_cast<float>(d + 360) * 3600 - 1296e3) / 3600;
}

inline double UniformCatmullRom(const double a, const double b, const double c, const double d, double t)
{
    return .5 * (((3 * (b - c) + d - a) * t + 2 * a - 5 * b + 4 * c - d) * t + c - a) * t + b;
}

inline double Lerp(const double a, const double b, const double t)
{
    return a * (1 - t) + b * t;
}

static const TIFFFieldInfo Fields[] = {{42113, -1, -1, TIFF_ASCII, FIELD_CUSTOM, true, false, const_cast<char *>("GDAL_NODATA")}};

namespace
{
    static auto _ = []() {
        static TIFFExtendProc p = TIFFSetTagExtender([](TIFF *t) {
            TIFFMergeFieldInfo(t, Fields, sizeof(Fields) / sizeof(TIFFFieldInfo));
            if (p)
                p(t);
        });
        return true;
    }();
}

template <typename T, typename U>
shared_ptr<Tile<T, U>> Tile<T, U>::Load(istream &stream)
{
    XTIFFInitialize();
    // TODO: read GDAL_NODATA
    auto xtif = unique_ptr<TIFF, function<void(TIFF *)>>{TIFFStreamOpen("Tile", &stream), bind(TIFFClose, placeholders::_1)};
    if (xtif)
    {
        const auto x = xtif.get();
        uint16_t s;
        double *tps, *pss;
        uint32_t w, h;
        if (TIFFGetField(x, TIFFTAG_SAMPLESPERPIXEL, &s) && s == 1 &&
            TIFFGetField(x, TIFFTAG_SAMPLEFORMAT, &s) && (s == SAMPLEFORMAT_INT || s == SAMPLEFORMAT_UINT || s == SAMPLEFORMAT_IEEEFP) &&
            TIFFGetField(x, TIFFTAG_BITSPERSAMPLE, &s) && s == sizeof(T) * 8 &&
            TIFFGetField(x, TIFFTAG_GEOTIEPOINTS, &s, &tps) && s == 6 && !tps[0] && !tps[1] && !tps[2] && !tps[5] &&
            TIFFGetField(x, TIFFTAG_GEOPIXELSCALE, &s, &pss) && s == 3 && !pss[2] &&
            TIFFGetField(x, TIFFTAG_IMAGEWIDTH, &w) && TIFFGetField(x, TIFFTAG_IMAGELENGTH, &h))
        {
            const auto dx{RoundD(pss[0])}, dy{RoundD(pss[1])};
            const size_t length{w * h};
            auto data = new T *[h];
            size_t y = 0;
            const auto strips = TIFFNumberOfStrips(x);
            for (uint32_t i{}; i < strips; ++i)
            {
                T *ts = new T[TIFFStripSize(x) / sizeof(T)];
                if (!ts)
                    abort();
                const auto read = TIFFReadEncodedStrip(x, i, ts, -1);
                if (read < 0)
                    throw runtime_error{"Could not decode a TIFF strip"};
                const auto rows = read / sizeof(T) / w;
                for (size_t j{}; j < rows; ++j)
                    data[y++] = ts + w * j;
            }
            const auto x0{tps[3]}, y0{tps[4]};
            xtif = nullptr;
            return shared_ptr<Tile>{new Tile{static_cast<size_t>(w), static_cast<size_t>(h), w % 2 ? RoundD(x0 + dx / 2) : x0, h % 2 ? RoundD(y0 - dy / 2) : y0, dx, dy, const_cast<const T **>(data)}};
        }
        throw runtime_error{"Unsupported GeoTIFF format"};
    }
    throw runtime_error{"Could not open the TIFF"};
}

template <typename T, typename U>
Tile<T, U>::~Tile()
{
    for (size_t i{}; i < h_; ++i)
        delete[] data_[i];
    delete[] data_;
}

template <typename T, typename U>
T Tile<T, U>::Get(size_t x, size_t y) const
{
    return data_[y][x];
}

template <typename T, typename U>
double Tile<T, U>::Interpolate(const double lat, const double lon, const bool bicubic) const
{
    const double x = (lon - x0) * dx_1_, y = (y0 - lat) * dy_1_, rx = x - floor(x), ry = y - floor(y);
    const auto x_ = static_cast<size_t>(x), y_ = static_cast<size_t>(y),
               x2 = clamp(x_, size0, w_), y2 = clamp(y_, size0, h_),
               x3 = clamp(x_ + 1, size0, w_), y3 = clamp(y_ + 1, size0, h_);
    if (bicubic)
    {
        const auto x1 = clamp(x_ - 1, size0, w_), y1 = clamp(y_ - 1, size0, h_),
                   x4 = clamp(x_ + 2, size0, w_), y4 = clamp(y_ + 2, size0, h_);
        return UniformCatmullRom(
            UniformCatmullRom(data_[y1][x1], data_[y1][x2], data_[y1][x3], data_[y1][x4], rx),
            UniformCatmullRom(data_[y2][x1], data_[y2][x2], data_[y2][x3], data_[y2][x4], rx),
            UniformCatmullRom(data_[y3][x1], data_[y3][x2], data_[y3][x3], data_[y3][x4], rx),
            UniformCatmullRom(data_[y4][x1], data_[y4][x2], data_[y4][x3], data_[y4][x4], rx),
            ry);
    }
    else
    {
        return Lerp(
            Lerp(data_[y2][x2], data_[y2][x3], rx),
            Lerp(data_[y3][x2], data_[y3][x3], rx),
            ry);
    }
}

template class Tile<int8_t>;
template class Tile<>;
template class Tile<float>;