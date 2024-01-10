#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ble_server.h"

class FifoCom
{
public:
  ~FifoCom() { }
  static void startFifoThread(const std::string &fifoName);
  static void initServer(std::shared_ptr<BleServer> server);
  static void sendFifo(const std::string &data);
  static void closeFifo();

private:
  FifoCom(const std::string &fifoName);
  void readFifo();

private:
  static FifoCom* ins_;
  std::string fifoName_;
  std::shared_ptr<BleServer> server_;
  bool isReading_;
  std::fstream fifoStream_;
};