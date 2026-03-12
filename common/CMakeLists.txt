add_library(common STATIC
    protocol/protocol.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/third_party
)

target_compile_features(common PUBLIC cxx_std_17)
