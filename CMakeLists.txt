cmake_minimum_required(VERSION 3.18)

project(s2dem VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

find_library(S2_FOUND s2 REQUIRED)
find_library(GEOGRAPHIC_FOUND Geographic REQUIRED)
find_library(GEOTIFF_FOUND geotiff REQUIRED)
find_library(TIFFXX_FOUND tiffxx REQUIRED)
find_library(TIFF_FOUND tiff REQUIRED)

add_executable(s2dem main.cxx tile.cxx)
configure_file(config.h.in config.h)
target_include_directories(s2dem PUBLIC "${PROJECT_BINARY_DIR}")
target_link_libraries(s2dem PRIVATE "${S2_FOUND}" "${GEOGRAPHIC_FOUND}" "${GEOTIFF_FOUND}" "${TIFFXX_FOUND}" "${TIFF_FOUND}" pthread)

install(TARGETS s2dem DESTINATION bin)