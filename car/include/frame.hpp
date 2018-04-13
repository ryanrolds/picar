#pragma once

#include <mutex>

struct Frame {
  std::mutex frameMutex;
  unsigned char* frame;
  int frameSize;

  Frame(int size);
  int getSize();
  void set(unsigned char* buffer);
  void get(unsigned char* buffer); 
};
