project(GaijinTest)

cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party_libs/rapidjson/include)

add_library(dictionary_server
  src/server/storage.cpp
  src/server/server.cpp
  src/server/connection.cpp
)

add_library(dictionary_client
  src/client/client.cpp
)

add_executable(dictionary_server_main
  src/server/main.cpp
)

add_executable(dictionary_client_cmd
  src/client/cmd_client.cpp
)

add_executable(dictionary_load_client
  src/client/load_test_client.cpp
)

target_link_libraries(dictionary_server_main PRIVATE dictionary_server)
target_link_libraries(dictionary_client_cmd PRIVATE dictionary_client)
target_link_libraries(dictionary_load_client PRIVATE dictionary_client)
