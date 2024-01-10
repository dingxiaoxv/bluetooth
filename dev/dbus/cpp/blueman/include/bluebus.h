#ifndef DM_BLUE_BUS_H
#define DM_BLUE_BUS_H

#include <string>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>


namespace dm {

struct BlueDevice {
  std::string name;
  std::string address;
  std::string path;
  int16_t rssi;
  bool trusted;
  bool paired;
  bool connected;
  bool pair_setd;

  BlueDevice() {
    rssi = 0;
    trusted = false;
    paired = false;
    connected = false;
    pair_setd = false;
  }
};

class Bluebus {
public:
  Bluebus();
  ~Bluebus();
  void run();
  void scan(bool on);
  void sendAdapterCommand(const std::string& cmd);

  BlueDevice& device() { return dev_; }
  void sendDeviceCommand(const std::string& cmd);
  int addObject(sd_bus_message* msg);
  int getDevice(sd_bus_message* msg, BlueDevice& dev);
  void pairDevice();

private:
  bool init();
  void addAdapter(sd_bus_message* msg, const char* path);

private:
  sd_bus* bus_;
  sd_bus_slot* slot_;
  std::string adaPath_;
  BlueDevice dev_;
  bool scaning_;
};

}

#endif // DM_BLUE_BUS_H