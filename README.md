# s2dem
Re-project DEM onto S2Cell

## Data Preparation
1. Download digital elevation models (DEM) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm
2. Unzip
3. Install geoid from https://geographiclib.sourceforge.io/html/geoid.html#geoidinst
   ```sh
   geographiclib-get-geoids egm96-5
   ```

## Format
```h
struct header
{
   float value_offset, value_scale;
   uint64_t begin_cell_id, end_cell_id;
};
```

## Build
- Replace `std::ios::streamoff` with `std::streamoff` in `Geoid.hpp`