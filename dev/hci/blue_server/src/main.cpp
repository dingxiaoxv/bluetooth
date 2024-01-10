#include <unistd.h>
#include <iostream>
#include <thread>
#include "bluez/hci.h"
#include "bluez/hci_lib.h"
#include "bluez/mainloop.h"
#include "bluez/timeout.h"

#include "ble_server.h"
#include "hci_helper.h"
#include "fifo_com.h"

static bool timeout_hander(void *user_data) {
  std::cout << "timer out" << std::endl;
  std::cout << "timer thread id: " << std::this_thread::get_id() << std::endl;
  return true;
}

int main(int argc, const char* argv[]) {
  std::string advName(argv[1]);

  HciHelper hci;
  if (!hci.valid()) {
    std::cerr << "failed to open hci device";
    return -1;
  }

  do
  {
    auto hciVersion = hci.getHciVersion();
    std::cout << "Using advertising name: " << advName << std::endl;;
    hci.setLeAdvertisingData(0x0a0a, advName.c_str());

    mainloop_init();
    // timeout_add(1000, timeout_hander, nullptr, nullptr);
    std::shared_ptr<BleServer> server = std::make_shared<BleServer>(advName, 512);
    if (server->connectionEstablished()) {
      std::cout << "connection established" << std::endl;;
      server->initServices();
    } else {
      return -1;
    }
    FifoCom::startFifoThread("bluetooth_fifo");
    FifoCom::initServer(server);
    
    mainloop_run();
    FifoCom::closeFifo();
    sleep(1);
  } while (true);

  return 0;
}
