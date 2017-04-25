#include "detect.hpp"

#include <raspicam/raspicam_cv.h>
#include <iostream>

raspicam::RaspiCam_Cv camera;
cv::CascadeClassifier cascade;
Detection detection;
int has_detection;
std::mutex detection_mutex;

const double FRAME_SCALE = 3.0d;

int setupCamera() {
  std::cout << "Camera processing starting" << std::endl;

  if (!camera.open()) {
    std::cout << "Capture from camera didn't work" << std::endl;
    return -1;
  } 

  cascade.load("visionary.net_cat_cascade_web_LBP.xml");
  
  std::cout << "Setup complete!" << std::endl;
  
  return 0;  
}

void processCamera() {
  cv::Mat frame;
  cv::Mat frame1;
  int result;

  result = setupCamera();
  if (result == -1) {
    return;
  }
 
  for(;;) {
    camera.grab();
    camera.retrieve(frame);

    if (frame.empty()) {
      std::cout << "Emptry frame, exiting" << std::endl;
      break;
    }
    
    frame1 = frame.clone();

    //std::cout << "Frame width: "  << frame1.cols << std::endl; 
    
    cv::flip(frame1, frame1, -1); // Flip image
    result = processFrame(frame1);

    int c = cv::waitKey(10);
    if (c == 27 || c == 'q' || c == 'Q') {
      break;
    }
  }
};

int processFrame(cv::Mat& frame) {
  std::vector<cv::Rect> faces;
  double ticks;

  const static cv::Scalar colors[] = {
    cv::Scalar(255,0,0),
    cv::Scalar(255,128,0),
    cv::Scalar(255,255,0),
    cv::Scalar(0,255,0),
    cv::Scalar(0,128,255),
    cv::Scalar(0,255,255),
    cv::Scalar(0,0,255),
    cv::Scalar(255,0,255)
  };
  cv::Mat gray, smallImg;
  
  cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
  double fx = 1 / FRAME_SCALE;
  cv::resize(gray, smallImg, cv::Size(), fx, fx, cv::INTER_LINEAR);
  cv::equalizeHist(smallImg, smallImg);
  int radius;

  //ticks = (double) cvGetTickCount();
  
  // Detect
  cascade.detectMultiScale(smallImg, faces, 1.4, 3, 0
			   //|CASCADE_FIND_BIGGEST_OBJECT
			   //|CASCADE_DO_ROUGH_SEARCH
  			   |cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30));

  //ticks = (double) cvGetTickCount() - ticks;
  //std::cout << "Detection time: " << ticks / (double) (cvGetTickFrequency() * 1000) << std::endl;

  if (faces.size() > 0) {
    //std::cout << "Frame width: " << smallImg.cols << std::endl;
    setDetection(faces[0], smallImg.cols, smallImg.rows);
  } else {
    //clearDetection();
  }
  
  for (size_t i = 0; i < faces.size(); i++) {
    cv::Rect r = faces[i];
    cv::Scalar color = colors[i%8];
    cv::Point center;
    int radius;    
    double aspectRatio;

    aspectRatio = (double) r.width / r.height;
    center.x = cvRound(r.x + r.width * 0.5);
    center.y = cvRound(r.y + r.height * 0.5);
    radius = cvRound((r.width + r.height) * 0.25);
    color = colors[i % 8];
    
    circle(smallImg, center, radius, color, 3, 8, 0);
  }

  //imshow("result", smallImg);
  
  return 0;
};

int setDetection(cv::Rect d, int width, int height) {
  std::unique_lock<std::mutex> lock(detection_mutex); 

  detection = {d, width, height};
  has_detection = 1;

  lock.unlock();
  
  return 0;
};
  
int clearDetection() {
  std::unique_lock<std::mutex> lock(detection_mutex); 

  resetDetection();

  lock.unlock();
  
  return 0;
};

Detection getDetection() {
  return detection;
}
  
int hasDetection() {
  return has_detection;
}

int resetDetection() {
  return has_detection = 0;
}
