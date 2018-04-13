#include "frame.hpp"
#include <cstring>

Frame::Frame(int size) {
  frameSize = size;
  frame = new unsigned char[frameSize]; 
};

int Frame::getSize() {
  return frameSize;
};

void Frame::set(unsigned char* buffer) {
  std::lock_guard<std::mutex> frameGuard(frameMutex);
  std::memcpy(buffer, frame, frameSize);
};

void Frame::get(unsigned char* buffer) {
  std::lock_guard<std::mutex> frameGuard(frameMutex);
  std::memcpy(frame, buffer, frameSize);
};
