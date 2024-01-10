#ifndef DM_BLE_SERVER_H
#define DM_BLE_SERVER_H

#include "bluez/bluetooth.h"
#include "bluez/uuid.h"
#include "bluez/gatt-db.h"
#include "bluez/gatt-server.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>


class BleServer {
public:
  BleServer(const std::string &deviceName, int mtu);
  ~BleServer();
  void initServices();
  void svcChanged();
  bool connectionEstablished() { return fd_ >= 0; }
  std::string getDeviceName() { return deviceName_; }
  void response(const std::vector<uint8_t> response);
  void notify(const std::vector<uint8_t> notification);
  void processFifo(const std::string &data);
  void processFifoNotify();
  void processFifoResponse();
  void transReadResponse(gatt_db_attribute* attrib, unsigned int id, uint16_t offset);
  void transWriteResponse(gatt_db_attribute* attrib, unsigned int id, uint16_t offset,
                    const uint8_t* value, size_t len, uint8_t opcode, bt_att* att);             

private:
  void populateGapService();
  void populateGattService();
  void populateCustomService();

private:
  const std::string deviceName_;
  int fd_;
  gatt_db *db_;
  bt_att *att_;
  bt_gatt_server *gatt_;
  int mtuSize_;
  gatt_db_attribute *svcChngd_;
  gatt_db_attribute *attrib_;
  uint16_t handle_;
  uint16_t gattSvcChngdHandle_;
  bool indicate_;

  const int kResponseSize = 1024;
  std::vector<uint8_t> response_;
  const int kRequestSize = 1024;
  std::vector<uint8_t> request_;
  int requestLen_;

  std::mutex respMutex_;
  std::condition_variable condResp_;

  int notifyfd_;
  std::vector<uint8_t> fifoNotifyQueue_;
  std::mutex notifyMutex_;

  int readfd_;
  std::vector<uint8_t> fifoRespQueue_;
  std::mutex readMutex_;
};


#endif // DM_BLE_SERVER_H