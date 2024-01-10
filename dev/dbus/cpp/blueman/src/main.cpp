#include "bluebus.h"
#include <thread>

void onRequest(dm::Bluebus* blue, const std_msgs::String::ConstPtr& msg) {
  const std::string& cmd = msg->data;
  if (cmd == "exit") {
    exit(0);
  }

  // StartDiscovery, StopDiscovery, RemoveDevice is adpater command
  if (cmd == "StartDiscovery" || cmd == "StopDiscovery") {// || cmd == "RemoveDevice") {
    blue->sendAdapterCommand(cmd);
    return;
  }
  blue->sendDeviceCommand(cmd);
}

void bluekeyConnected(bool paired, bool connected) {
  std_msgs::Int16 msg;
  msg.data = (paired ? 0x01 : 0x00) | (connected ? 0x02 : 0x00);
  g_pub_state.publish(msg);
  cLOG_INFO << "..... " << paired << ".... " << connected << ", " << msg.data;
}

int main(int argc, char** argv) {
  dm::Bluebus blue;

  std::thread t([&blue] {
    blue.scan(true);
    blue.run();
  });
  t.detach();

  return 0;
}