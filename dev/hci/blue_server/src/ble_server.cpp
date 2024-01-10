#include "ble_server.h"
#include "bluez/bluetooth.h"
#include "bluez/l2cap.h"
#include "bluez/uuid.h"
#include "bluez/att.h"
#include "bluez/gatt-server.h"
#include "bluez/mainloop.h"
#include "bluez/util.h"
#include "json_packer.h"

#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/eventfd.h>

#pragma pack(push)
#pragma pack(1)
struct TransPdu {
  uint8_t head;
  uint16_t len;
  uint8_t data[0];
  uint8_t tail;
};
#pragma pack(pop)

#define UUID_GAP  0x1800
#define UUID_GATT	0x1801
const int PDU_EXCEPT = 4;

static uint8_t checksum(const uint8_t* data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  return sum + 0x80;
}

static int64_t getLocalTimeStamp() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string vectorToHexString(const std::vector<uint8_t> &vec)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (const auto &value : vec)
  {
      oss << std::setw(2) << static_cast<int>(value);
  }
  return oss.str();
}

static void onAttDisconnectCallback(int err, void *user_data)
{
  std::cerr << "Device disconnected: " << strerror(err) << std::endl;
	mainloop_quit();
  std::cout << std::endl;
}

static void onGapDeviceNameReadCallback(struct gatt_db_attribute *attrib,
                  unsigned int id, uint16_t offset,
                  uint8_t opcode, struct bt_att *att,
                  void *user_data) {
  BleServer* server = (BleServer*)user_data;
  uint8_t error = 0;
  size_t len = 0;
  const uint8_t *value = NULL;
  std::string name = server->getDeviceName();
  len = name.size();
  do
  {
    if (offset > len) {
      error = BT_ATT_ERROR_INVALID_OFFSET;
      break;
    }
    len -= offset;
    value = len ? (const uint8_t*) name.c_str() : NULL;
  } while (false);
  gatt_db_attribute_read_result(attrib, id, error, value, len);
}

static void onGapDeviceNameWriteCallback(struct gatt_db_attribute *attrib,
					unsigned int id, uint16_t offset,
					const uint8_t *value, size_t len,
					uint8_t opcode, struct bt_att *att,
					void *user_data) {
  uint8_t error = 0;
  std::cout << "GAP Device Name Write called" << std::endl;
  gatt_db_attribute_write_result(attrib, id, error);
}

static void onGapDeviceNameExtPropReadCallback(struct gatt_db_attribute *attrib,
					unsigned int id, uint16_t offset,
					uint8_t opcode, struct bt_att *att,
					void *user_data) {
  uint8_t value[2];
  std::cout << "Device Name Extended Properties Read called" << std::endl;
	value[0] = BT_GATT_CHRC_EXT_PROP_RELIABLE_WRITE;
	value[1] = 0;
	gatt_db_attribute_read_result(attrib, id, 0, value, sizeof(value));
}

static void onGattServiceChangedCallback(struct gatt_db_attribute *attrib,
					unsigned int id, uint16_t offset,
					uint8_t opcode, struct bt_att *att,
					void *user_data)
{
	std::cout << "Service Changed Read called" << std::endl;
	gatt_db_attribute_read_result(attrib, id, 0, NULL, 0);
}

static void onConfCallback(void *user_data)
{
	std::cout << "received svc changed confirmation" << std::endl;
}

static void onTransReadCallback(gatt_db_attribute *attrib, unsigned int id, uint16_t offset, 
					          uint8_t opcode, bt_att *att, void *user_data) {
  BleServer* server = (BleServer*)user_data;
  if (!attrib) {
    std::cerr << "gatt attrib is null" << std::endl;
    return;
  }
  if (!server) {
    std::cerr << "blue server is null" << std::endl;
    return;
  }
  server->transReadResponse(attrib, id, offset);
}

static void onTransWriteCallback(gatt_db_attribute *attrib, unsigned int id, uint16_t offset, 
					          const uint8_t *value, size_t len, uint8_t opcode, bt_att *att, void *user_data) {
  BleServer* server = (BleServer*)user_data;
  if (!attrib) {
    std::cerr << "gatt attrib is null" << std::endl;
    return;
  }
  if (!server) {
    std::cerr << "blue server is null" << std::endl;
    gatt_db_attribute_write_result(attrib, id, BT_ATT_ERROR_UNLIKELY);
  }
  server->transWriteResponse(attrib, id, offset, value, len, opcode, att);
}

static void confCallback(void *user_data)
{
	std::cout << "received indicate confirmation" << std::endl;
}

static void onNotifyTask(int fd, uint32_t events, void *user_data) {
  uint64_t one;
  if (read(fd, &one, sizeof(one)) != sizeof(one)) {
    std::cerr << "notification read error" << std::endl;
    return;
  }
  BleServer* server = (BleServer*)user_data;
  server->processFifoNotify();
}

static void onReadTask(int fd, uint32_t events, void *user_data) {
  uint64_t one;
  if (read(fd, &one, sizeof(one)) != sizeof(one)) {
    std::cerr << "map data read error" << std::endl;
    return;
  }
  BleServer* server = (BleServer*)user_data;
  server->processFifoResponse();
}

BleServer::BleServer(const std::string &deviceName, int mtu)
  : fd_(-1), att_(NULL), db_(NULL), indicate_(false), mtuSize_(mtu) {
  notifyfd_ = eventfd(0, EFD_NONBLOCK);
  readfd_ = eventfd(0, EFD_NONBLOCK);

  mainloop_add_fd(notifyfd_, EPOLLIN | EPOLLERR | EPOLLET, onNotifyTask, this, NULL);
  mainloop_add_fd(readfd_, EPOLLIN | EPOLLERR | EPOLLET, onReadTask, this, NULL);
  
  int fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (fd < 0) {
    std::cerr << "Failed to create L2CAP socket" << std::endl;
    return;
  }

  struct sockaddr_l2 addr;
  memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_cid = htobs(4); // ATT_CID 4
  addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
  // memset(&addr.l2_bdaddr, 0, sizeof(addr.l2_bdaddr));

  do {
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      std::cerr << "Failed to bind L2CAP socket" << std::endl;
      break;
    }

    struct bt_security security = {
      .level = BT_SECURITY_LOW,
      // .key_size = 16,
    };
    if (setsockopt(fd, SOL_BLUETOOTH, BT_SECURITY, &security, sizeof(security)) != 0) {
      std::cerr << "Failed to set L2CAP security level" << std::endl;
      break;
    }

    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
      std::cerr << "enable reuse address failed" << std::endl;
      break;
    }

    if (listen(fd, 10) < 0) {
      std::cerr << "Listening on socket failed" << std::endl;
      break;
    }

    std::cout << "wait for connection on ATT channel" << std::endl;
    struct sockaddr_l2 peer;
    memset(&peer, 0, sizeof(peer));
    socklen_t len = sizeof(peer);
    fd_ = accept(fd, (struct sockaddr *)&peer, &len);
    if (fd_ < 0) {
      std::cerr << "Failed to accept L2CAP connection" << std::endl;
      break;
    }
  } while (0);

  close(fd);

  if (fd_ < 0) {
    std::cerr << "Failed to establish L2CAP connection" << std::endl;
    return;
  }

  att_ = bt_att_new(fd_, 0);
  if (!att_) {
    std::cerr << "Failed to allocate ATT" << std::endl;
    return;
  }

  if (!bt_att_set_close_on_unref(att_, true)) {
    std::cerr << "Failed to set up ATT transport layer" << std::endl;
    return;
  }

  if (!bt_att_register_disconnect(att_, onAttDisconnectCallback, NULL, NULL)) {
		std::cerr << "Failed to set ATT disconnect handler" << std::endl;
		return;
	}

  db_ = gatt_db_new();
  if (!db_) {
    std::cerr << "Failed to allocate GATT database" << std::endl;
    return;
  }

  gatt_ = bt_gatt_server_new(db_, att_, mtuSize_, 0);
  if (!gatt_) {
    std::cerr << "Failed to allocate GATT server" << std::endl;
    return;
  }

  mtuSize_ = bt_att_get_mtu(att_) - 1;
  if (!mtuSize_) {
    std::cerr << "Failed to get MTU size" << std::endl;
    return;
  }
}

BleServer::~BleServer() {
  close(notifyfd_);
  close(readfd_);
  close(fd_);
}

void BleServer::initServices() {
  std::cout << ">>>>>>>> begin init bluetooth services <<<<<<<<" << std::endl;
  populateGapService();
  populateGattService();
  populateCustomService();
  std::cout << ">>>>>>>> init bluetooth services end <<<<<<<<" << std::endl;
}

void BleServer::response(const std::vector<uint8_t> response) {
  {
    std::lock_guard<std::mutex> lock(respMutex_);
    response_.clear();
    response_.resize(kResponseSize);
    memset(&response_[0], 0, kResponseSize);
    TransPdu* pdu = (TransPdu*)&response_[0];
    pdu->head = 0xc0;
    int pduLen = 0;
    if (response.size() > kResponseSize - PDU_EXCEPT) {
      pduLen = kResponseSize - PDU_EXCEPT;
    } else {
      pduLen = response.size();
    }
    pdu->len = htons(pduLen);
    memcpy(pdu->data, response.data(), pduLen);
    int totalLen = pduLen + PDU_EXCEPT;
    response_[totalLen - 1] = 0xc0;
    response_.resize(totalLen);
  }
  condResp_.notify_one();

  std::vector<uint8_t> notification(1, 0);
  notify(notification);
}

void BleServer::notify(const std::vector<uint8_t> notification) {
  std::vector<std::vector<uint8_t>> packets;
  for (auto it = notification.begin(); it < notification.end(); it += mtuSize_) {
    if (it + mtuSize_ < notification.end()) {
      packets.push_back(std::vector<uint8_t>(it, it + mtuSize_));
    } else {
      packets.push_back(std::vector<uint8_t>(it, notification.end()));
    }
  }

  for (const auto& packet : packets) {
    if (indicate_) {
      if (!bt_gatt_server_send_indication(gatt_, handle_, packet.data(), packet.size(), confCallback, NULL, NULL)) {
        std::cerr << "Failed to initiate indication" << std::endl;
        return;
      }
    } else {
      if (!bt_gatt_server_send_notification(gatt_, handle_, packet.data(), packet.size(), false)) {
        std::cerr << "Failed to initiate notification" << std::endl;
        return;
      }
    }
  }
}

void BleServer::processFifo(const std::string &msg) {
  rapidjson::Document doc;
  doc.Parse(msg.c_str());
  if (doc.HasParseError()) {
    std::cerr << "messgae parse error: " << msg << std::endl;
    return;
  }

  if (!doc.HasMember("topic")) {
    std::cerr << "no topic, messgae: " << msg << std::endl;
    return;
  }

  std::string topic(doc["topic"].GetString());
  if (topic == "response") {
    std::string data(doc["data"].GetString());
    std::vector<uint8_t> vec(data.begin(), data.end());
    {
      std::lock_guard<std::mutex> guard(respMutex_);
      fifoRespQueue_.reserve(fifoRespQueue_.size() + vec.size());
      fifoRespQueue_.insert(fifoRespQueue_.end(), vec.begin(), vec.end());
    }
    std::cout << "data: " << vectorToHexString(fifoRespQueue_) << std::endl;

    uint64_t one = 1;
    write(readfd_, &one, sizeof(one));
    
  } else if (topic == "notification") {
    std::string data(doc["data"].GetString());
    std::vector<uint8_t> vec(data.begin(), data.end());
    {
      std::lock_guard<std::mutex> guard(notifyMutex_);
      fifoNotifyQueue_.reserve(fifoNotifyQueue_.size() + vec.size());
      fifoNotifyQueue_.insert(fifoNotifyQueue_.end(), vec.begin(), vec.end());
    }
    
    uint64_t one = 1;
    write(notifyfd_, &one, sizeof(one));
  }
}

void BleServer::processFifoNotify() {
  std::vector<uint8_t> vec;
  {
    std::lock_guard<std::mutex> guard(notifyMutex_);
    vec.swap(fifoNotifyQueue_);
  }
  notify(vec);
}

void BleServer::processFifoResponse() {
  std::vector<uint8_t> vec;
  {
    std::lock_guard<std::mutex> guard(respMutex_);
    vec.swap(fifoRespQueue_);
  }
  response(vec);
}

void BleServer::transReadResponse(gatt_db_attribute *attrib, unsigned int id, uint16_t offset)
{
  if (response_.empty()) {
    gatt_db_attribute_read_result(attrib, id, 0, nullptr, 0);
    return;
  }

  std::unique_lock<std::mutex> lock(respMutex_);
  condResp_.wait(lock, [this] { return !response_.empty(); });

  if( 0 == offset ) {
    gatt_db_attribute_read_result(attrib, id, 0, &response_[0], response_.size());
  }
  else if ( offset > 0 && offset < response_.size() ) {
    gatt_db_attribute_read_result(attrib, id, 0, &response_[offset], response_.size() - offset);
  }
}

void BleServer::transWriteResponse(gatt_db_attribute *attrib, unsigned int id, uint16_t offset, 
                    const uint8_t *value, size_t len, uint8_t opcode, bt_att *att) {
  std::vector<uint8_t> vec;
  vec.resize(len);
  memcpy(vec.data(), value, len);
  std::cout << "recv raw data from client:" << vectorToHexString(vec) << std::endl;
  gatt_db_attribute_write_result(attrib, id, 0);
}

void BleServer::svcChanged() {
  uint16_t start, end;
	uint8_t value[4];
  // if (!gatt_db_attribute_get_service_handles(svcChngd_, &start, &end)) {
	// 	std::cerr << "Failed to obtain changed service handles";
	// 	return;
	// }
  start = 0x0001;
  end = 0xffff;

  uint16_t handle = gatt_db_attribute_get_handle(svcChngd_);
  if (!handle) {
		std::cerr << "Failed to obtain handles for characteristic" << std::endl;
		return;
	}

	put_le16(start, value);
	put_le16(end, value + 2);

  bt_gatt_server_send_indication(gatt_, handle, value, 4, onConfCallback, NULL, NULL);
}

void BleServer::populateGapService() {
  bt_uuid_t uuid;

  /* Add the GAP service */
  bt_uuid16_create(&uuid, UUID_GAP);
  auto svc = gatt_db_add_service(db_, &uuid, true, 6);
  // add Device Name characteristic
  bt_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
  gatt_db_service_add_characteristic(svc, &uuid,
          BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
          BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_EXT_PROP,
          onGapDeviceNameReadCallback,
          onGapDeviceNameWriteCallback,
          this);

  bt_uuid16_create(&uuid, GATT_CHARAC_EXT_PROPER_UUID);
	gatt_db_service_add_descriptor(svc, &uuid, 
          BT_ATT_PERM_READ,
					onGapDeviceNameExtPropReadCallback,
					NULL,
          this);
  gatt_db_service_set_active(svc, true);
  std::cout << "GAP service init!" << std::endl;
}

void BleServer::populateGattService() {
  bt_uuid_t uuid;
  /* Add the GATT service */
	bt_uuid16_create(&uuid, UUID_GATT);
	auto svc = gatt_db_add_service(db_, &uuid, true, 4);

	bt_uuid16_create(&uuid, GATT_CHARAC_SERVICE_CHANGED);
	svcChngd_ = gatt_db_service_add_characteristic(svc, &uuid,
	        BT_ATT_PERM_READ,
			    BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_INDICATE,
			    onGattServiceChangedCallback,
			    NULL, this);
	gattSvcChngdHandle_ = gatt_db_attribute_get_handle(svcChngd_);

	gatt_db_service_set_active(svc, true);

  /* add Device Information service */
  bt_uuid16_create(&uuid, 0x180a);
  svc = gatt_db_add_service(db_, &uuid, true, 4);
  gatt_db_service_set_active(svc, true);

  std::cout << "GATT service init!" << std::endl;
}

void BleServer::populateCustomService() {
  bt_uuid_t uuid;
  /* add test service */ 
  bt_uuid16_create(&uuid, 0x0a0a);
  auto svc = gatt_db_add_service(db_, &uuid, true, 4);

  // add trans characteristic
  bt_uuid16_create(&uuid, 0x0001);
  attrib_ = gatt_db_service_add_characteristic(svc, &uuid, BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                  BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_WRITE | BT_GATT_CHRC_PROP_NOTIFY, onTransReadCallback, onTransWriteCallback, this);
  handle_ = gatt_db_attribute_get_handle(attrib_);

  gatt_db_service_set_active(svc, true);
  std::cout << "test service init!" << std::endl;
}

