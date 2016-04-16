#include "main.hpp"

const double FRAME_SCALE = 4.0d; // Adjust for performance

raspicam::RaspiCam_Cv camera;
cv::Mat frame;
cv::Mat frame1;
cv::CascadeClassifier cascade, nestedCascade;
vector<cv::Rect> faces
cv::Rect r;
Point center;
double aspectRatio;
Scalar color;

const static Scalar colors[] = {
  Scalar(255,0,0),
  Scalar(255,128,0),
  Scalar(255,255,0),
  Scalar(0,255,0),
  Scalar(0,128,255),
  Scalar(0,255,255),
  Scalar(0,0,255),
  Scalar(255,0,255)
};

cv::Mat gray, smallImg;

int main(int argc, const chat** argv) {
  int result;

  result = setup();
  if (result == -1) {
    return -1;
  }

  // Start camera threads
  // AI loop
};

int setup() {
  if (!camera.open()) {
    std::cout << "Capture from camera didn't work" << std::endl;
    return -1;
  } 

  return 0;  
}

void processCamera() {
  std::cout << "Camera processing starting" << std::endl;

  int result;
  
  for(;;) {
    camera.grab();
    camera.retrieve(frame);

    if (frame.empty()) {
      std::cout << "Emptry frame, exiting" << std::endl;
      break;
    }

    frame1 = frame.clone();
    cv::flip(frame1, frame1, 1); // Flip image
    result = processFrame(frame1);
  }
};

int processFrame(cv::Mat& frame) {
  cvtColor(frame, gray, COLOR_BGR2GRAY);
  double fx = 1 / FRAME_SCALE;
  resize(, smallImg, Size(), fx, fx, INTER_LINEAR);
  equalizeHist(smallImg, smallImg);
  
  t = (double) cvGetTickCount();
  
  // Detect
  cascade.detectMultiScale(smallImg, faces, 1.1, 2, 0);

  t = (double) cvGetTickCount() - t;
  std::cout << "Detection time: " << t / (double) cvGetTickFrequency() * 1000 << std::endl;

  for (size_t i = 0; i < faces.size(); i++) {
    r = faces[i];
    aspectRatio = (double) r.width / r.height;
    center.x = cv::cvRound(r.x + r.width * 0.5);
    center.y = cv::cvRound(r.y + r.height * 0.5);
    radius = cv::cvRound((r.width + r.height) * 0.25);
    color = colors[i % 8];
    
    circle(smallImg, center, radius, color, 3, 8, 0);
  }

  imshow("result", smallImg);
  
  return 0;
};
