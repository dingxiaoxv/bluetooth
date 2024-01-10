#ifndef DM_HCI_EXT_H
#define DM_HCI_EXT_H

#include <stdint.h>
#include <string>

#pragma pack(push)
#pragma pack(1)
struct LeAdvertisingFlags {
  const uint8_t length = 0x02;
  const uint8_t type = 0x01;
  const uint8_t data = 0x02; // LE General Discoverable Mode
};

struct LeAdvertisingService {
  const uint8_t length = 0x03;
  const uint8_t type = 0x02; // incomplete list of 16-bit Service Class UUIDs
  uint8_t uuid[2];
};

struct LeAdvertisingServiceData {
  const uint8_t length = 0x17;
  const uint8_t type = 0x16; // Service Data - 16-bit UUID
  uint8_t uuid[2];
  uint8_t data[20];
};

struct LeAdvertisingLocalName {
  const uint8_t length = 0x17;
  const uint8_t type = 0x09;
  uint8_t name[22];
};


struct LeAdvertising {
  const uint8_t length = 0x1f; // 31 bytes
  LeAdvertisingFlags flags;
  LeAdvertisingService service;
  // LeAdvertisingServiceData serviceData;
  LeAdvertisingLocalName localName;
};

struct LeAdvertisingExt {
  const uint8_t handle = 0x01;
  const uint8_t dataOperation = 0x03; // 0x00: complete data, 0x01: incomplete data, 0x02: unchanged data
  const uint8_t fragmentPreference = 0x01; // 0x00: no fragmentation, 0x01: enable fragmentation
  const uint8_t dataLength = 0x1b; // 27 bytes
  LeAdvertisingServiceData serviceData;
  LeAdvertisingFlags flags;
};

struct LeAdvertisingEnable {
  uint8_t enable;
  const uint8_t setNum = 0x01;
  const uint8_t handle = 0x01;
  const uint16_t duration = 0x0000;
  const uint8_t maxExtAdvEvents = 0x00;
};
#pragma pack(pop)


enum HCI_VERSION {
  HCI_BT_UNKNOWN = 0x00,
  HCI_BT_4_0 = 0x06,
  HCI_BT_4_1 = 0x07,
  HCI_BT_4_2 = 0x08,
  HCI_BT_5_0 = 0x09,
  HCI_BT_5_1 = 0x0a,
  HCI_BT_5_2 = 0x0b,
  HCI_BT_5_3 = 0x0c,
};


class HciHelper {
public:
  HciHelper();
  ~HciHelper();
  bool valid() const { return fd_ >= 0; }
  HCI_VERSION getHciVersion();
  void setLeAdvertisingData(uint16_t service, const char *deviceName);
  void setLeAdvertisingDataExt(uint16_t service, const char* data);
  std::string getMacAddress();

private:
  int dev_;
  int fd_;
};


#endif // DM_HCI_EXT_H