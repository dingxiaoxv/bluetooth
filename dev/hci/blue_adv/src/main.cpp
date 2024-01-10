#include "bluez/hci.h"
#include "bluez/hci_lib.h"
#include "bluez/mainloop.h"
#include "bluez/timeout.h"
#include <iostream>
#include <thread>
#include <unistd.h>
#include <csignal>
#include "hci_helper.h"

void signalHandler(int signum) {
  std::cout << "Interrupt signal (" << signum << ") received.\n";

  std::exit(signum);
}

static bool timeout_hander(void *user_data) {
  std::cout << "timer out" << std::endl;
  std::cout << "timer thread id: " << std::this_thread::get_id() << std::endl;
  return true;
}

int main(int argc, const char *argv[]) {
  std::string advName(argv[1]);
  std::signal(SIGINT, signalHandler);

  HciHelper hci;
  if (!hci.valid()) {
    std::cerr << "failed to open hci device";
    return -1;
  }

  auto hciVersion = hci.getHciVersion();
  std::cout << "Using advertising name: " << advName << std::endl;
  hci.setLeAdvertisingData(0xfe97, advName);
  mainloop_init();
  mainloop_run();

  return 0;
}
