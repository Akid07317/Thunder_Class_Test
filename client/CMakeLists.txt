set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt6 OPTIONAL_COMPONENTS Multimedia)

add_executable(thunder_client
    src/main.cpp
    src/main_window.cpp
    src/network/tcp_client.cpp
    src/pages/login_page.cpp
    src/pages/admin_page.cpp
    src/pages/teacher_page.cpp
    src/pages/student_page.cpp
)

target_include_directories(thunder_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(thunder_client PRIVATE
    common
    Qt6::Widgets
    Qt6::Network
)

if(TARGET Qt6::Multimedia)
    target_link_libraries(thunder_client PRIVATE Qt6::Multimedia)
    target_compile_definitions(thunder_client PRIVATE HAS_QT_MULTIMEDIA=1)
    message(STATUS "Qt6 Multimedia found — audio capture/playback enabled")
else()
    message(STATUS "Qt6 Multimedia not found — audio UI only (no capture/playback)")
endif()
