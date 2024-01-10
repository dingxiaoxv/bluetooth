#include "device_info.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rapidjson/document.h>
#include <cnr/logging.h>
#include <cnr/ifstream.h>
#include <regex>

const std::string DID_FILE = "/mnt/private/ULI/factory/did.txt";
const std::string KEY_FILE = "/mnt/private/ULI/factory/key.txt";
const std::string PINCODE_FILE = "/mnt/private/ULI/factory/pincode.txt";
const std::string OS_RELEASE = "/etc/os-release";
const std::string MODEL = "dreame.mower.p2255";
const std::string WLAN_NAME = "wlan0";


static const std::string readFile(const std::string &fileName) {
  cnr::IFStream ifs(fileName.c_str());
  char buf[1024] = { 0 };
  ifs.read(buf, sizeof(buf) - 1);
  return buf;
}

namespace dm {
const std::string getWlanMAC() {
  struct ifreq ifr;
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sockfd == -1) {
    return "";
  }

  std::memset(&ifr, 0, sizeof(ifr));
  std::strcpy(ifr.ifr_name, WLAN_NAME.c_str());

  if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
    close(sockfd);
    return "";
  }

  close(sockfd);
  unsigned char mac_addr[6];
  std::memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);

  char mac_str[18];
  std::sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  return mac_str;
}

const std::string getVersion() {
  std::string str;
  try {
    str = readFile(OS_RELEASE);
  } catch(cnr::Error& e) {
    cLOG_ERROR << e.description();
  }
  
  rapidjson::Document doc;
  doc.Parse(str.c_str());
  if (doc.HasParseError()) {
    return "";
  }

  if (doc.HasMember("fw_arm_ver") == false) {
    return "";
  }

  return doc["fw_arm_ver"].GetString();
}

DeviceInfo::DeviceInfo(HciHelper& hci, bool isPairNetMode) {
#ifdef HOST_MOWER
  char *tmpdid = getenv("MOWER_DID");
  char *tmpkey = getenv("MOWER_KEY");
  did = tmpdid ? tmpdid : "1234567890";
  key = tmpkey ? tmpkey : "1234567890";
  pincode = "7788";
  mac = hcimac; // use same mac on host
  model = "dreame.mower.p2255";
  version = "1.0.0_1111";
#else
  try {
    did = readFile(DID_FILE);
    key = readFile(KEY_FILE);
    pincode = readFile(PINCODE_FILE);
    mac = getWlanMAC();
    version = getVersion();
    model = MODEL;
  } catch(cnr::Error& e) {
    cLOG_ERROR << e.description();
  }
#endif

  std::regex regexPattern(std::string(1, '-'));
  std::string did_str = std::regex_replace(std::string(did), regexPattern, "");
  hcimac = hci.getMacAddress();
  if (isPairNetMode) {
    name = "dreamebt-10152-" + hcimac.substr(hcimac.size() - 5);
  } else {
    name = "dreamebt-" + did_str + "-" + hcimac.substr(hcimac.size() - 5);
  }
}

} // namespace dm