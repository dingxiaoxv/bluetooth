#include "fifo_com.h"
#include <thread>
#include <chrono>
#include <unistd.h>

FifoCom* FifoCom::ins_ = nullptr;

void FifoCom::startFifoThread(const std::string &fifoName)
{
  std::thread t([fifoName] {
    FifoCom fifo(fifoName);
    ins_ = &fifo;
    ins_->readFifo();
    std::cout << "fifo thread start" << std::endl;
  });
  t.detach();
}

void FifoCom::initServer(std::shared_ptr<BleServer> server) {
  if (ins_) {
    ins_->server_ = server;
  }
}

void FifoCom::sendFifo(const std::string &data) {
  if (ins_) {
    std::fstream fs;
    fs.open(ins_->fifoName_, std::ios::out);
    if (!ins_->fifoStream_) {
      std::cerr << "Failed to open FIFO for writing." << std::endl;
      return;
    }
    if (ins_->fifoStream_) {
      ins_->fifoStream_ << data << std::endl;
      ins_->fifoStream_.flush();
    }
    fs.close();
  }
}

void FifoCom::closeFifo() {
  if (ins_) {
    ins_->fifoStream_.close();
    // 删除FIFO文件
    unlink(ins_->fifoName_.c_str());
    ins_->server_ = nullptr;
  }
}

FifoCom::FifoCom(const std::string &fifoName) : fifoName_(fifoName), isReading_(true) {
  if (mkfifo(fifoName.c_str(), 0666) == -1) {
    std::cerr << "Failed to create FIFO." << std::endl;
    return;
  }
}

void FifoCom::readFifo()
{
  while (isReading_) {
    fifoStream_.open(fifoName_, std::ios::in);
    if (!fifoStream_) {
      // std::cerr << "Failed to open FIFO for reading." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    std::cout << "fifo open successfully" << std::endl;

    std::string data;
    if (fifoStream_) {
      std::getline(fifoStream_, data);
    }
    std::cout << "recv from fifo: " << data << std::endl;
    server_->processFifo(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
