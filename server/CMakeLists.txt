find_package(Qt6 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)

add_executable(thunder_server
    src/main.cpp
    src/tcp_server.cpp
    src/user_store.cpp
    src/session_manager.cpp
    src/class_manager.cpp
    src/question_manager.cpp
    src/statistics_exporter.cpp
)

target_include_directories(thunder_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(thunder_server PRIVATE common Qt6::Core Qt6::Network)
