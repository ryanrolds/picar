#include "camera.hpp"

#include <stdexcept>
#include <iostream>

const unsigned int CAPTURE_WIDTH = 320;
const unsigned int CAPTURE_HEIGHT = 240;

CameraHandler::CameraHandler() {};

// Prepare camera
void CameraHandler::prepare() {
  camera.setCaptureSize(CAPTURE_WIDTH, CAPTURE_HEIGHT);
  camera.setVerticalFlip(true);
  camera.setHorizontalFlip(true);
  camera.setExposure(raspicam::RASPICAM_EXPOSURE_AUTO); // Max 30
  camera.setFormat(raspicam::RASPICAM_FORMAT_RGB);
  
  if (!camera.open()) {
    throw std::runtime_error("Error: Unable to open camera");
  }
  
  std::cout << "Camera: " << camera.getId() << std::endl;

  int frameSize = getFrameSize();
  std::cout << "Frame size: " << frameSize << std::endl;
  std::cout << "Frame dim: " << camera.getWidth() << "x" << camera.getHeight() << std::endl;
};

void CameraHandler::run(Frame& frame) {
  std::cout << "Camera processing starting" << std::endl;

  int frameSize = camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_RGB);
  unsigned char *buffer = new unsigned char[frameSize];

  while(true) {
    // Get frame
    camera.grab();
    camera.retrieve(buffer);

    // Write frame 
    frame.set(buffer); 
  }
}

int CameraHandler::getFrameSize() {
  return camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_RGB);
}

int CameraHandler::getHeight() {
  return camera.getHeight();
};

int CameraHandler::getWidth() {
  return camera.getWidth();
};
