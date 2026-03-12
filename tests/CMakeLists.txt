add_executable(test_login_client
    test_login_client.cpp
)
target_link_libraries(test_login_client PRIVATE common)

add_executable(test_class_client
    test_class_client.cpp
)
target_link_libraries(test_class_client PRIVATE common)

add_executable(test_question_client
    test_question_client.cpp
)
target_link_libraries(test_question_client PRIVATE common)

add_executable(test_stats_client
    test_stats_client.cpp
)
target_link_libraries(test_stats_client PRIVATE common)

add_executable(test_random_call_client
    test_random_call_client.cpp
)
target_link_libraries(test_random_call_client PRIVATE common)

add_executable(test_screen_share_client
    test_screen_share_client.cpp
)
target_link_libraries(test_screen_share_client PRIVATE common)

add_executable(test_audio_client
    test_audio_client.cpp
)
target_link_libraries(test_audio_client PRIVATE common)

add_executable(test_edge_cases
    test_edge_cases.cpp
)
target_link_libraries(test_edge_cases PRIVATE common)
