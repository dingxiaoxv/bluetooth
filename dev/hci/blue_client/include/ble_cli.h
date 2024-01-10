#ifndef BLE_CLI_H
#define BLE_CLI_H

#include "bluez/bluetooth.h"
#include "bluez/gatt-db.h"
#include "bluez/gatt-client.h"

#include <vector>

class BleClient
{
public:
  BleClient(bdaddr_t *src, bdaddr_t *dst, uint16_t mtu);
  ~BleClient() {}
  gatt_db *db() { return db_; }
  bool connectionEstablished() { return fd_ > 0; }
  void write(size_t cnt);

private:
  int fd_;
  gatt_db *db_;
  bt_att *att_;
  bt_gatt_client *gatt_;
  int mtuSize_;
};

#endif