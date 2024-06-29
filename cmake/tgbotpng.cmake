include(cmake/FindWebP.cmake)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
add_library_san(TgBotPNG SHARED
    src/pictures/libPNG.cpp
    src/pictures/libWEBP.cpp
    src/pictures/libJPEG.cpp)
target_include_directories(TgBotPNG PUBLIC ${PNG_INCLUDE_DIR} ${WebP_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})
target_include_directories(TgBotPNG PUBLIC src/pictures)
target_link_libraries(TgBotPNG ${PNG_LIBRARIES} ${WebP_LIBRARIES} ${JPEG_LIBRARIES})