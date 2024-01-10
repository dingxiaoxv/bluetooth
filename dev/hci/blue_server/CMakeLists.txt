cmake_minimum_required(VERSION 3.0.2)
project(blue_server)

set_directory_properties(PROPERTIES COMPILE_OPTIONS "-g")
include_directories(include)

add_executable(blue_server 
                       src/main.cpp
                       src/ble_server.cpp
                       src/hci_helper.cpp
                       src/fifo_com.cpp
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

target_link_libraries(blue_server pthread)
