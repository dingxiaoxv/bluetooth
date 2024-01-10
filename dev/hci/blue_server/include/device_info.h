#ifndef DM_DEVICE_INFO_H
#define DM_DEVICE_INFO_H

#include "hci_helper.h"

namespace dm {
struct DeviceInfo {
  std::string did;
  std::string key;
  std::string pincode;
  std::string mac;
  std::string hcimac;
  std::string version;
  std::string model;
  std::string name;

  DeviceInfo(HciHelper& hci, bool isPairNetMode);
};

} // namespace dm

#endif // DM_DEVICE_INFO_H