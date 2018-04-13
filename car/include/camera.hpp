#include "frame.hpp"

#include <raspicam/raspicam.h>

class CameraHandler {
  raspicam::RaspiCam camera;

  public:
  CameraHandler();
  void prepare();
  void run(Frame &frame);
  int getFrameSize();
  int getHeight();
  int getWidth();
  void run();
};
