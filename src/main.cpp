#include "main.hpp"
#include "detect.hpp"
#include "control.hpp"
#include "senses.hpp"

#include <time.h>
#include <iostream>
#include <thread>
#include <unistd.h>
int setup();

int running = 1;

int main(int argc, const char** argv) {
  int result;
  int file;
  Detection target;

  // TODO remove file from main, push fd handling down into control
  file = setup();
  if (file == -1) {
    return -1;
  }
  
  // Start camera threads
  std::thread videoThread(processCamera);

  std::unique_lock<std::mutex> lock(detection_mutex);
  lock.unlock();

  cv::Point center;
  //__s16 step = 1;
  __u16 position = STEERING_CEN;
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 300000000L;
    
  // AI loop
  for(;;) {
    lock.lock();

    if (hasDetection() == 1) {
      target = getDetection();
      resetDetection();
      
      lock.unlock();
      
      center.x = cvRound(target.object.x + target.object.width * 0.5);
      center.y = cvRound(target.object.y + target.object.height * 0.5);

      double areaWidth = target.width * 1.0d;
      double steeringRange = 145.0d;
      double adjust = steeringRange / areaWidth;
      double offset = center.x * adjust;
      //std::cout << "Detection center " << center.x << ", " << center.y << std::endl;
      //std::cout << "Target " << target.object.y << ", " << target.object.y << std::endl;
      //std::cout << "Conversion " << target.width << ", " << steeringRange << ", " << adjust << std::endl;
      //std::cout << "Detection offset " << offset << std::endl;

      position = STEERING_MIN + offset;

      std::cout << "Position " << position << std::endl;
      
      setSteering(file, position);
      setForward(file, 4000);      
    } else {
      lock.unlock();

      int obst = hasObstical();
      std::cout << "Obst " << obst << std::endl;
      if (obst == 0) {
	setSteering(file, position);
	setForward(file, 4000);
      } else {
	setStop(file);
      }
    }

    nanosleep(&ts, NULL);
  }

  setStop(file);  
  cleanupControl(file);

  return 0;
};

int setup() {
  // Prepare controls/outputs (wheels, steering)
  int file = setupControl();
  if (file == -1) {
    std::cout << "Setup control error" << std::endl;
    return -1;
  }
  
  // Prepare senses/inputs
  int result = setupSenses();
  if (result == -1) {
    std::cout << "Setup senses error" << std::endl;
    return -1;
  }
  
  return file;
};
