#include "main.hpp"
#include "detect.hpp"
#include "control.hpp"

#include <time.h>
#include <iostream>
#include <thread>
#include <unistd.h>
int setup();

int running = 1;

int main(int argc, const char** argv) {
  int result;
  int file;
  cv::Rect target;
    
  result = setup();
  if (result == -1) {
    return -1;
  }

  file = setupControl();
  if (file == -1) {
    std::cout << "Setup error" << std::endl;
    return -1;
  }

  // Start camera threads
  std::thread videoThread (processCamera);

  cv::Point center;
  __s16 step = 1;
  __u16 position = STEERING_CEN;
  setSteering(file, STEERING_CEN);
  setStop(file);
  
  std::unique_lock<std::mutex> lock(detection_mutex);
  lock.unlock();

  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 200000000L;
    
  // AI loop
  for(;;) {
    lock.lock();

    if (hasDetection() == 1) {
      target = getDetection();
      resetDetection();
      
      lock.unlock();
      
      center.x = cvRound(target.x + target.width * 0.5);
      center.y = cvRound(target.y + target.height * 0.5);

      double offset = (center.x - 170 * 1.0d) * 0.25d;
      //std::cout << "Detection center " << center.x << ", " << center.y << std::endl;
      //std::cout << "Target " << target.y << ", " << target.y << " " <<  target.width << std::endl;
      //std::cout << "Detection offset " << offset << std::endl;

      position = STEERING_CEN + offset;

      std::cout << "Position " << position << std::endl;
      
      setSteering(file, position);
      setForward(file, 4000);      
    } else {
      lock.unlock();

      setStop(file);
    }

    nanosleep(&ts, NULL);
  }

  setStop(file);  
  cleanupControl(file);

  return 0;
};

int setup() {};
