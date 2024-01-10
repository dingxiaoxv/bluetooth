#include <unistd.h>
#include <iostream>
#include <getopt.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include "bluez/mainloop.h"
#include "ble_cli.h"

bdaddr_t any_bdaddr = {0, 0, 0, 0, 0, 0};
#define BDADDR_ANY (&any_bdaddr)

static struct option long_options[] = {
	{ "dest",		1, 0, 'd' },
	{ "mtu",		1, 0, 'm' },
	{ }
};

void testWrite(BleClient *client) {
  std::this_thread::sleep_for(std::chrono::seconds(10));
  for (size_t i = 0; i < 100; i++)
  {
    client->write(i);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

int main(int argc, char *argv[]) {
  int opt;
	uint16_t mtu = 0;
	bdaddr_t src_addr, dst_addr;

  while ((opt = getopt_long(argc, argv, "m:d:", long_options, NULL)) != -1) {
    switch (opt) {
    case 'm':
        int arg;
        arg = atoi(optarg);
        if (arg <= 0) {
          fprintf(stderr, "Invalid MTU: %d\n", arg);
          return EXIT_FAILURE;
        }

        if (arg > UINT16_MAX) {
          fprintf(stderr, "MTU too large: %d\n", arg);
          return EXIT_FAILURE;
        }
        
        mtu = (uint16_t)arg;
        break;
    case 'd':
        if (str2ba(optarg, &dst_addr) < 0) {
          fprintf(stderr, "Invalid remote address: %s\n",
                    optarg);
          return EXIT_FAILURE;
        }

        bacpy(&src_addr, BDADDR_ANY);
        break;
    default:
      fprintf(stderr, "Invalid option: %c\n", opt);
      return EXIT_FAILURE;
    }
  }

  mainloop_init();
  BleClient client(&src_addr, &dst_addr, mtu);
  if (!client.connectionEstablished()) {
    return -1;
  }

  // std::thread t(testWrite, &client);
  // t.detach();

	mainloop_run();
	printf("\n\nShutting down...\n");
  
  return 0;
}
