cmake_minimum_required(VERSION 3.17)
project(ffmpegDemo)

set(CMAKE_CXX_STANDARD 11)

include_directories("D:/thridpartys/ffmpeg/win64/include")

add_executable(ffmpegDemo main.cpp)

target_link_libraries(ffmpegDemo
        D:/thridpartys/ffmpeg/win64/lib/avcodec.lib
        D:/thridpartys/ffmpeg/win64/lib/avdevice.lib
        D:/thridpartys/ffmpeg/win64/lib/avfilter.lib
        D:/thridpartys/ffmpeg/win64/lib/avformat.lib
        D:/thridpartys/ffmpeg/win64/lib/avutil.lib
        D:/thridpartys/ffmpeg/win64/lib/swresample.lib
        D:/thridpartys/ffmpeg/win64/lib/swscale.lib
        )