#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <mutex>

extern std::mutex detection_mutex;

struct Detection {
  cv::Rect object;
  int width;
  int height;
};

void processCamera();
int processFrame(cv::Mat&);
Detection getDetection();
int setDetection(cv::Rect, int, int);
int clearDetection();
int hasDetection();
int resetDetection();
