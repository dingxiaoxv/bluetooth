#include "bluebus.h"

extern void bluekeyConnected(bool paired, bool connected);

using namespace dm;

static constexpr char BUS_NAME[] = "org.bluez";
static constexpr char BUS_PATH[] = "/org/bluez";
static constexpr char BUS_MAN[] = "org.bluez.AgentManager1";
static const char BUS_AGENT[] = "org.bluez.Agent1";
static const char BUS_ADAPTER[] = "org.bluez.Adapter1";
static const char BUS_DEVICE[] = "org.bluez.Device1";
static const char DBUS_MAN[] = "org.freedesktop.DBus.ObjectManager";

int connectFinshed(sd_bus_message *m, void *userdata, sd_bus_error *error) {
  if (userdata) {
    Bluebus* p = (Bluebus*) userdata;
    cLOG_INFO << "connect finished .......";
    return 0;
  }
  return -1;
}

int pairedFinshed(sd_bus_message *m, void *userdata, sd_bus_error *error) {
  if (userdata) {
    Bluebus* p = (Bluebus*) userdata;
    cLOG_INFO << "pair finished .......";
    return 0;
  }
  return -1;
}
int onDeviceAdded(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
  if (userdata) {
    Bluebus* p = (Bluebus*) userdata;
    p->addObject(msg);
    return 0;
  }
  return -1;
}

int onDeviceUpdated(sd_bus_message* msg, void* userdata, sd_bus_error* err) {
  if (userdata) {
    Bluebus* p = (Bluebus*) userdata;
    const char *itf;

    sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &itf); // TODO: check!!
    p->getDevice(msg, p->device());
    cLOG_INFO << "update Xenon device, address = " << p->device().address << ", rssi "
              << p->device().rssi << ", paired: " << p->device().paired << ", connected: "
              << p->device().connected;

    if (!p->device().pair_setd && !p->device().paired) {
      p->pairDevice();
      p->device().pair_setd = true;
    }

    bluekeyConnected(p->device().paired, p->device().connected);

    return 0;
  }
  return -1;
}

int pairing_agent(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  return 0;
}

Bluebus::Bluebus() : scaning_(false) {
  if (!init()) {
    // TODO: how to relase these points?
    slot_ = nullptr;
    bus_ = nullptr;
  }
}

Bluebus::~Bluebus() {}

bool Bluebus::init() {
  if (sd_bus_default_system(&bus_) < 0) {
    cLOG_ERROR << "failed to get system bus";
    return false;
  }

  if (sd_bus_add_object(bus_, &slot_, "/pairing", pairing_agent, NULL) < 0) {
    cLOG_ERROR << "failed to add pairing object";
    return false;
  }

  if (sd_bus_call_method(bus_, BUS_NAME, BUS_PATH, BUS_MAN, "RegisterAgent", NULL, NULL, "os", "/pairing", "KeyboardOnly") < 0) {
    cLOG_ERROR << "failed to register agent";
    return false;
  }

  const char* m = "type='signal', sender='org.bluez', path='/', interface='org.freedesktop.DBus.ObjectManager', member='InterfacesAdded'";
  if (sd_bus_add_match(bus_, &slot_, m, onDeviceAdded, this) < 0) {
    cLOG_ERROR << "failed to add matched signal";
    return false;
  }

  // TODO InterfacesRemoved event!!

  sd_bus_message *msg = NULL, *r = NULL;
  if (sd_bus_message_new_method_call(bus_, &r, BUS_NAME, "/", DBUS_MAN, "GetManagedObjects") < 0) {
    cLOG_ERROR << "failed to create method call";
    return false;
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  if (sd_bus_call(bus_, r, 0, &error, &msg) < 0) {
    cLOG_ERROR << "bus call failed";
    return false;
  }

  sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, NULL);
  while (!sd_bus_message_at_end(msg, 0)) {
    sd_bus_message_enter_container(msg, SD_BUS_TYPE_DICT_ENTRY, NULL);
    addObject(msg);
    sd_bus_message_exit_container(msg);
  }
  sd_bus_message_exit_container(msg);

  return true;
}

int Bluebus::addObject(sd_bus_message* msg) {
  const char* path;
  if (sd_bus_message_read_basic(msg, SD_BUS_TYPE_OBJECT_PATH, &path) < 0) {
    cLOG_ERROR << "failed to get object path";
    return -1;
  }

  cLOG_DEBUG << "object path is " << path;

  if (sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, NULL) < 0) {
    cLOG_ERROR << "failed to enter container: ARRAY";
    return -1;
  }

  while (!sd_bus_message_at_end(msg, 0)) {
    if (sd_bus_message_enter_container(msg, SD_BUS_TYPE_DICT_ENTRY, NULL) < 0) {
      cLOG_ERROR << "failed to enter container: DICT ENTRY!";
      return -1;
    }

    const char* interface;
    if (sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &interface) < 0) {
      cLOG_ERROR << "failed to get object interface";
    }

    cLOG_DEBUG << "object interface is " << interface;

    if (strcmp(interface, BUS_ADAPTER) == 0) {
      addAdapter(msg, path);
    } else if (strcmp(interface, BUS_DEVICE) == 0) {
      BlueDevice dev;
      dev.rssi = 0;
      getDevice(msg, dev);
      if (dev.name.find("Xenon") == 0) {
        cLOG_INFO << "find Xenon device, address = " << dev.address << ", rssi " << dev.rssi << ", paired: " << dev.paired << ", connected: " << dev.connected;
        if (dev_.name.empty()) {
          dev_ = dev;
          dev_.path = path;
          cnr::MStream ms;
          ms.fmt("type='signal', sender='org.bluez', path='%s', interface='org.freedesktop.DBus.Properties', member='PropertiesChanged'", path);
          cLOG_INFO << ms;
          if (sd_bus_add_match(bus_, &slot_, ms.data(), onDeviceUpdated, this) < 0) {
            cLOG_ERROR << "failed to add matched signal for PropertiesChanged";
            return -1;
          }
          if (scaning_) pairDevice();
        }
      }

    } else {
      // TODO: GATT ?
      sd_bus_message_skip(msg, NULL);
    }

    sd_bus_message_exit_container(msg);
  }
  sd_bus_message_exit_container(msg);


  return 0;
}

void Bluebus::addAdapter(sd_bus_message* msg, const char* path) {
  // only support one adapter
  sd_bus_message_skip(msg, NULL);
  adaPath_ = path;
}

int Bluebus::getDevice(sd_bus_message* msg, BlueDevice& dev) {
  if (sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, NULL) < 0) {
    cLOG_ERROR << "failed to enter type array";
    return -1;
  }

  const char* name;
  char t;
  const char* type;
  while (!sd_bus_message_at_end(msg, 0)) {
    sd_bus_message_enter_container(msg, SD_BUS_TYPE_DICT_ENTRY, NULL);

    sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &name);

    sd_bus_message_peek_type(msg, &t, &type);

    sd_bus_message_enter_container(msg, SD_BUS_TYPE_VARIANT, NULL);

    if (!strcmp(name, "Paired") && !strcmp(type, "b")) {
      int value;
      sd_bus_message_read_basic(msg, 'b', &value);
      dev.paired = value > 0;
    } else if (!strcmp(name, "Connected") && !strcmp(type, "b")) {
      int value;
      sd_bus_message_read_basic(msg, 'b', &value);
      dev.connected = value > 0;
    } else if (!strcmp(name, "RSSI") && !strcmp(type, "n")) {
      int16_t value;
      sd_bus_message_read_basic(msg, 'n', &value);
      dev.rssi = value;
    } else if (!strcmp(name, "Address") && !strcmp(type, "s")) {
      char* value = NULL;
      sd_bus_message_read_basic(msg, 's', &value);
      dev.address = value;
    } else if (!strcmp(name, "Name") && !strcmp(type, "s")) {
      char* value = NULL;
      sd_bus_message_read_basic(msg, 's', &value);
      dev.name = value;
    } else if (!strcmp(name, "Trusted") && !strcmp(type, "b")) {
      int value;
      sd_bus_message_read_basic(msg, 'b', &value);
      dev.trusted = value > 0;
    } else {
      sd_bus_message_skip(msg, type);
    }

    sd_bus_message_exit_container(msg);
    sd_bus_message_exit_container(msg);
  }

  sd_bus_message_exit_container(msg);

  return 0;
}

int timerEventCallback(sd_event_source *s, uint64_t usec, void *userdata) {
  Bluebus* p = (Bluebus*) userdata;
  bluekeyConnected(p->device().paired, p->device().connected);
  
  uint64_t now;
  sd_event_now(sd_event_source_get_event(s), CLOCK_MONOTONIC, &now);
  sd_event_add_time(sd_event_source_get_event(s), &s, CLOCK_MONOTONIC,
      now + 1000000, 0, timerEventCallback, userdata);
  return 0;
}

void Bluebus::run() {
  sd_event *e;
  sd_event_default(&e);
  sd_bus_attach_event(bus_, e, 0);

  sd_event_source* timerSource = nullptr;
  uint64_t now;
  sd_event_now(e, CLOCK_MONOTONIC, &now);
  sd_event_add_time(e, &timerSource, CLOCK_MONOTONIC, now + 1000000, 0, timerEventCallback, this);
  sd_event_loop(e);
}

void Bluebus::scan(bool on) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *r = NULL;
  sd_bus_call_method(bus_, BUS_NAME, adaPath_.c_str(), BUS_ADAPTER, on ? "StartDiscovery" : "StopDiscovery", &error, &r, NULL);
}

void Bluebus::sendAdapterCommand(const std::string& cmd) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *r = NULL;
  if (cmd == "StartDiscovery") {
    scaning_ = true;
  } else if (cmd == "StopDiscovery") {
    scaning_ = false;
  }
  cLOG_INFO << "Got adapter command: " << cmd << ", adpater = " << adaPath_;
  // sd_bus_call_method(bus_, BUS_NAME, adaPath_.c_str(), BUS_ADAPTER, "StartDiscovery", &error, &r, NULL);
}

void Bluebus::sendDeviceCommand(const std::string& cmd) {
  cLOG_INFO << cmd << " " << dev_.path;
  sd_bus_call_method_async(bus_, NULL, BUS_NAME, dev_.path.c_str(), BUS_DEVICE, cmd.c_str(), connectFinshed, this, NULL);
}

void Bluebus::pairDevice() {
  cLOG_INFO << "pair to " << dev_.path;
  sd_bus_call_method_async(bus_, NULL, BUS_NAME, dev_.path.c_str(), BUS_DEVICE, "Pair", pairedFinshed, this, NULL);
  dev_.pair_setd = true;
}