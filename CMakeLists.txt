cmake_minimum_required(VERSION 3.16)
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Min macOS")
    endif()
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac archs")
    endif()
endif()
project(claudegame C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT WIN32 AND NOT APPLE)
    find_package(OpenSSL REQUIRED)
endif()
if(WIN32)
    set(_CG_BUNDLED_RAYLIB_ROOTS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/raylib-5.5_win64_mingw-w64
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib
    )
    find_path(RAYLIB_INCLUDE_DIR raylib.h
        HINTS ${_CG_BUNDLED_RAYLIB_ROOTS} ${RAYLIB_DIR} ${CMAKE_PREFIX_PATH} ENV RAYLIB_DIR
        PATH_SUFFIXES include
        NO_DEFAULT_PATH)
    find_library(RAYLIB_LIBRARY NAMES raylib libraylib raylib.dll
        HINTS ${_CG_BUNDLED_RAYLIB_ROOTS} ${RAYLIB_DIR} ${CMAKE_PREFIX_PATH} ENV RAYLIB_DIR
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
        find_package(raylib CONFIG QUIET)
        if(NOT raylib_FOUND)
            find_path(RAYLIB_INCLUDE_DIR raylib.h PATH_SUFFIXES include)
            find_library(RAYLIB_LIBRARY NAMES raylib libraylib PATH_SUFFIXES lib)
        endif()
    endif()
    if(NOT raylib_FOUND AND (NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY))
        message(FATAL_ERROR "raylib not found. Install via vcpkg/MSYS2, or place a prebuilt raylib under third_party/raylib/raylib-5.5_win64_mingw-w64.")
    endif()
else()
    if(APPLE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib-mac/lib/libraylib.a)
        set(RAYLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib-mac/include CACHE PATH "" FORCE)
        set(RAYLIB_LIBRARY     ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib-mac/lib/libraylib.a CACHE FILEPATH "" FORCE)
        message(STATUS "Using bundled universal raylib at third_party/raylib-mac/")
    else()
        find_path(RAYLIB_INCLUDE_DIR raylib.h
            HINTS /opt/homebrew/Cellar/raylib/5.5/include /opt/homebrew/include)
        find_library(RAYLIB_LIBRARY raylib
            HINTS /opt/homebrew/Cellar/raylib/5.5/lib /opt/homebrew/lib)
        if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
            message(FATAL_ERROR "raylib not found. For universal Mac builds run mac/build-raylib-universal.sh first.")
        endif()
    endif()
endif()
set(CG_CLIENT_SOURCES
    src/main.cpp
    src/AuthManager.cpp
    src/Game.cpp
    src/LoginScreen.cpp
    src/Menu.cpp
    src/Leaderboard.cpp
    src/ChatRoom.cpp
    src/NetClient.cpp
    src/ConnectScreen.cpp
    src/Protocol.cpp
    src/Maps.cpp
    src/Store.cpp
    src/UiTheme.cpp
    src/I18n.cpp
    src/SoloMenu.cpp
    src/LocalMatch.cpp
    src/Bot.cpp
    src/Crypto.cpp
    src/Paths.cpp
    src/EmbeddedServer.cpp
    server/Server.cpp
    server/Match.cpp
)
set(CG_SERVER_SOURCES
    server/main.cpp
    server/Server.cpp
    server/Match.cpp
    src/AuthManager.cpp
    src/Protocol.cpp
    src/Maps.cpp
    src/Crypto.cpp
    src/Paths.cpp
)
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
    list(APPEND CG_CLIENT_SOURCES third_party/sqlite/sqlite3.c)
    list(APPEND CG_SERVER_SOURCES third_party/sqlite/sqlite3.c)
    set(CG_BUNDLED_SQLITE ON)
    message(STATUS "Using bundled sqlite3 amalgamation at third_party/sqlite/")
endif()
if(APPLE)
    set(CG_BUNDLE_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/Inter-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/Inter-Bold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NotoSansSC-Regular.otf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NotoSansSC-Bold.otf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NOTICE_FONTS.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    )
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mac/AppIcon.icns)
        list(APPEND CG_BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mac/AppIcon.icns)
    endif()
    add_executable(claudegame MACOSX_BUNDLE ${CG_CLIENT_SOURCES} ${CG_BUNDLE_RESOURCES})
    set_source_files_properties(${CG_BUNDLE_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(claudegame PROPERTIES
        OUTPUT_NAME "ClaudeGame"
        MACOSX_BUNDLE_BUNDLE_NAME "ClaudeGame"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jonzhu.claudegame"
        MACOSX_BUNDLE_COPYRIGHT "© 2026 jonzhu"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in"
    )
else()
    add_executable(claudegame ${CG_CLIENT_SOURCES})
endif()
target_include_directories(claudegame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)
if(NOT CG_BUNDLED_SQLITE)
    target_link_libraries(claudegame PRIVATE sqlite3)
else()
    target_include_directories(claudegame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
endif()
if(raylib_FOUND)
    target_link_libraries(claudegame PRIVATE raylib)
else()
    target_include_directories(claudegame PRIVATE ${RAYLIB_INCLUDE_DIR})
    target_link_libraries(claudegame PRIVATE ${RAYLIB_LIBRARY})
endif()
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(claudegame PRIVATE OpenSSL::Crypto)
endif()
if(APPLE)
    target_link_libraries(claudegame PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework OpenGL"
    )
elseif(WIN32)
    target_link_libraries(claudegame PRIVATE
        winmm gdi32 opengl32 ws2_32 crypt32 bcrypt
    )
    if(MINGW)
        target_link_options(claudegame PRIVATE -static-libgcc -static-libstdc++ -mwindows)
    endif()
endif()
add_executable(claudegame_server ${CG_SERVER_SOURCES})
target_include_directories(claudegame_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)
if(NOT CG_BUNDLED_SQLITE)
    target_link_libraries(claudegame_server PRIVATE sqlite3)
else()
    target_include_directories(claudegame_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
endif()
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(claudegame_server PRIVATE OpenSSL::Crypto)
endif()
if(WIN32)
    target_link_libraries(claudegame_server PRIVATE ws2_32 crypt32 bcrypt)
    if(MINGW)
        target_link_options(claudegame_server PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
