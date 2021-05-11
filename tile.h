#pragma once
#include <memory>

template <typename T = int16_t, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
class Tile
{
public:
    static std::shared_ptr<Tile> Load(std::istream &stream);

    const size_t width, height;
    const double x0, y0, dx, dy;

    ~Tile();

    T Get(size_t x, size_t y) const;

    double Peak(double lat, double lon) const;

private:
    const size_t w_, h_;
    const double dx_1_, dy_1_;
    const T **data_;

    Tile(const size_t width, const size_t height, const double x0, const double y0, const double dx, const double dy, const T **data) : width{width}, height{height}, x0{x0}, y0{y0}, dx{dx}, dy{dy}, w_{width / 2 * 2}, h_{height / 2 * 2}, dx_1_{1 / dx}, dy_1_{1 / dy}, data_{data} {}
};