#include "hci_helper.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include "bluez/bluetooth.h"
#include "bluez/hci.h"
#include "bluez/hci_lib.h"

HciHelper::HciHelper() : fd_(-1) {
  if (0 != system("hciconfig hci0 down")) {
    std::cerr << "shut down hci0 failed" << std::endl;
  }
  sleep(1);
  if (0 != system("hciconfig hci0 up")) {
    std::cerr << "bring up hci0 failed" << std::endl;
  }
  sleep(1);
  if (0 != system("hciconfig hci0 noleadv && hciconfig hci0 leadv")) {
    std::cerr << "set hci0 advertising failed" << std::endl;
  }

  dev_ = hci_get_route(NULL);
  if (dev_ < 0) {
    std::cerr << "failed to get the device id, error: " << strerror(errno);
    return;
  }

  fd_ = hci_open_dev(dev_);
  if (fd_ < 0) {
    std::cerr << "failed to open device, error: " << strerror(errno);
    return;
  }
}

HciHelper::~HciHelper() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

HCI_VERSION HciHelper::getHciVersion() {
  struct hci_version ver;
  int ret = hci_read_local_version(fd_, &ver, 2000);
  if (ret < 0) {
    std::cerr << "failed to read local hci version, error: " << strerror(errno);
    return HCI_BT_UNKNOWN;
  }

  if (ver.hci_ver < HCI_BT_4_0 || ver.hci_ver > HCI_BT_5_3) {
    std::cerr << "unsupported hci version: " << ver.hci_ver;
    return HCI_BT_UNKNOWN;
  }

  return (HCI_VERSION) ver.hci_ver;
}

void HciHelper::setLeAdvertisingData(uint16_t service, const char *deviceName) {
  struct hci_request rq;
  uint8_t status;
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  /*  Min advertising interval: 1280.000 msec (0x0800)
      Max advertising interval: 1280.000 msec (0x0800)
      Type: Connectable undirected - ADV_IND (0x00)
      Own address type: Public (0x00)
      Direct address type: Public (0x00)
      Direct address: 00:00:00:00:00:00 (OUI 00-00-00)
      Channel map: 37, 38, 39 (0x07)
      Filter policy: Allow Scan Request from Any, Allow Connect Request from Any (0x00)
  */
  uint8_t buf[LE_SET_ADVERTISING_PARAMETERS_CP_SIZE] = { 
    0x00, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00
  };
  rq.cparam = buf;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x06, error = " << strerror(errno);
    return;
  }

  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  uint8_t enable = 0x01;
  rq.cparam = &enable;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x0a, error = " << strerror(errno);
    return;
  }

  LeAdvertising adv;
  adv.service.uuid[0] = service & 0xff;
  adv.service.uuid[1] = (service >> 8) & 0xff;
  // adv.serviceData.uuid[1] = (service >> 8) & 0xff;
  // memcpy(adv.serviceData.data, data, 20);
  memset(adv.localName.name, 0, 22);
  memcpy(adv.localName.name, deviceName, 22);

  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.cparam = &adv;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, error = " << strerror(errno);
  }
}

void HciHelper::setLeAdvertisingDataExt(uint16_t service, const char* data) {
  struct hci_request rq;
  uint8_t status;
  rq.ogf = OGF_LE_CTL;
  rq.ocf = 0x36;
  rq.clen = 25;
  uint8_t buf[25] = { 
    0x01, 0x13, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08,
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7f, 0x01, 0x00, 0x01, 0x00,
    0x00
  };

  rq.cparam = buf;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x36, error = " << strerror(errno);
    return;
  }


  rq.ocf = 0x38;
  rq.clen = 4;
  uint8_t buf2[4] = { 0x01, 0x03, 0x01, 0x00 };
  rq.cparam = buf2;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x38, error = " << strerror(errno);
    return;
  }


  LeAdvertisingEnable enable;
  enable.enable = 0x01;
  rq.ogf = OGF_LE_CTL;
  rq.ocf = 0x39;
  rq.cparam = &enable;
  rq.clen = 6;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req when enable LE ext adv, error = " << strerror(errno);
    return;
  }

  LeAdvertisingExt adv;
  adv.serviceData.uuid[0] = service & 0xff;
  adv.serviceData.uuid[1] = (service >> 8) & 0xff;
  memcpy(adv.serviceData.data, data, 20);

  rq.ocf = 0x37;
  rq.cparam = &adv;
  rq.clen = 31;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req when set LE ext adv data, error = " << strerror(errno);
    return;
  }
}

std::string HciHelper::getMacAddress() {
  struct hci_dev_info dev_info;
  memset(&dev_info, 0, sizeof(dev_info));
  dev_info.dev_id = dev_;
  if (hci_devinfo(dev_, &dev_info) < 0) {
    std::cerr << "failed to get device info, error: " << strerror(errno);
    return "00:00:00:00:00:00";
  }

  char addr[18] = { 0 };
  ba2str(&dev_info.bdaddr, addr);
  return addr;
}