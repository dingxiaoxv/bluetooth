cmake_minimum_required(VERSION 3.0.2)
project(blue_client)

set_directory_properties(PROPERTIES COMPILE_OPTIONS "-g")
include_directories(include)

add_executable(blue_client 
                       src/main.cpp
                       src/ble_cli.cpp
                       src/bluez/att.c
                       src/bluez/hci.c
                       src/bluez/bluetooth.c
                       src/bluez/crypto.c
                       src/bluez/gatt-client.c
                       src/bluez/gatt-db.c
                       src/bluez/gatt-helpers.c
                       src/bluez/gatt-server.c
                       src/bluez/io-mainloop.c
                       src/bluez/mainloop.c
                       src/bluez/queue.c
                       src/bluez/timeout-mainloop.c
                       src/bluez/util.c
                       src/bluez/uuid.c
                       )

target_link_libraries(blue_client pthread)
