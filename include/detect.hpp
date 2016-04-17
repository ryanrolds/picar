#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <mutex>

extern std::mutex detection_mutex;

void processCamera();
int processFrame(cv::Mat&);
cv::Rect getDetection();
int setDetection(cv::Rect);
int clearDetection();
int hasDetection();
int resetDetection();
