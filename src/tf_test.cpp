#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <ctime>
#include <chrono>
#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/saved_model/loader.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"

int main(int argc, const char** argv) {

  // Setup CNN and other classification objects
  tensorflow::Scope root = tensorflow::Scope::NewRootScope();
  tensorflow::SavedModelBundle bundle;
  
  const std::string exportDir = "./models";
  tensorflow::SessionOptions sessOpts = tensorflow::SessionOptions();
  tensorflow::RunOptions runOpts = tensorflow::RunOptions();
  const std::unordered_set<std::string> tags = {"serve"};
  tensorflow::Status opStatus = tensorflow::LoadSavedModel(
    sessOpts, runOpts, exportDir, tags, &bundle
  );

  if (!opStatus.ok()) {
    std::cout << "Error " << opStatus.ToString() << "\n";
    throw std::runtime_error(opStatus.ToString());
  }

  // Image details
  int frameHeight = 240;
  int frameWidth = 320;
	const int height = 227;
	const int width = 227;
	const int depth = 3;

  // Load image in to char*
  //std::string testImage = "./frame_2017-07-25T21-04-31Z_910.ppm";
  std::string testImage = "/test/frame_2017-07-25T21-02-57Z_868.png";
  std::ifstream in(testImage);
  std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()); 
  const char* ppmFile = contents.c_str();

  int startOfLine = 0;
  int lineCount = 0;
  int p = 0;
  for(; p < strlen(ppmFile); p++) {
    if (ppmFile[p] == '\n') {
      lineCount++;
    }

    if (lineCount > 7) {
      break;
    }
  }

  //std::cout << "pos " << p << " " << ppmFile[p] << " " << ppmFile[p + 1] << std::endl;

  char* frame = new char[320*240*3];
  memset(frame, 0, 320*240*3);

  strncpy(frame, ppmFile + p + 1, 320*240*3);
  std::cout << frame[0] << std::endl;

  // Prepare image tensor for forward prop
  std::cout << "Pre tensor" << std::endl;

  tensorflow::Tensor image_uint(tensorflow::DT_UINT8, tensorflow::TensorShape({1, frameHeight, frameWidth, depth}));
  auto image_uint_mapped = image_uint.tensor<uint8_t, 4>();

  std::cout << "Post tensor" << std::endl;

  auto image_flat = image_uint.flat<uint8_t>();
  std::copy_n(frame, 320*240*3, image_flat.data());

  std::cout << "Post copy" << std::endl;

  std::cout << frame[0] << " " << frame[1] << " " << frame[2] << std::endl;
  std::cout << image_uint.DebugString() << std::endl;
  std::cout << image_uint.SummarizeValue(9) << std::endl;

  std::cout << "Post debug" << std::endl;

  /*
  // TODO iterator input_channels and write to x_mapped
  //int channel = 0;
  //std::vector<cv::Mat>::iterator it;
  //for (it = input_channels.begin() ; it != input_channels.end(); ++it) {
    const charfloat* source_data = frame;
    for (int y = 0; y < height; ++y) {
      const char* source_row = source_data + (y * width * depth);
      for (int x = 0; x < width; ++x) {
        const char* source_pixel = source_row + (x * depth); 
        for (int c = 0; c < depth; ++c) {
          const char* source_value = source_pixel + c; 
          image_uint_mapped(0, y, x, c) = *source_value;
        }
      }
    }

    //channel++;
  //}
  */

  //tensorflow::ops::cast(

  //std::vector<cv::Mat> input_channels;
  //cv::Mat image = cv::Mat(frameHeight, frameWidth, CV_8UC3, frame);
  
  //std::cout << "image_uint " << image_uint.DebugString() << std::endl;

  //std::string pngFile = "./frame_2017-07-25T21-04-31Z_910.png";
  std::string pngFile = "../../../picar/test/frame_2017-07-25T16-30-00Z_0.png";
  tensorflow::Output image_string = tensorflow::ops::ReadFile(root, pngFile);
  tensorflow::Output image_decode = tensorflow::ops::DecodePng(root, image_string);
  tensorflow::Output image_expand = tensorflow::ops::ExpandDims(root, image_decode, 0);
  tensorflow::Output image_resized = tensorflow::ops::CropAndResize(root,
      image_expand, {{0.0f, 0.0f, 1.0f, 1.0f}}, {0}, {227, 227});
  tensorflow::Output image_float = tensorflow::ops::Cast(root, image_resized,
      tensorflow::DT_FLOAT);  
  tensorflow::Output image_norm = tensorflow::ops::Subtract(root, image_float,
      {123.68f, 116.779f, 103.939f});
  tensorflow::Output image_bgr = tensorflow::ops::Reverse(root, image_norm, {-1});

  std::cout << "Run image" << std::endl;

  std::vector<tensorflow::Tensor> image_outputs;
  tensorflow::ClientSession session(root);
  session.Run({image_bgr}, &image_outputs);

  std::cout << "Post run image" << std::endl;

  tensorflow::Tensor x = image_outputs[0];

  std::cout << "Image" << x.DebugString() << std::endl;
  std::cout << x.SummarizeValue(9) << std::endl;

  /*
  cv::Mat imageFloat;
  image.convertTo(imageFloat, CV_32FC3);
  // TODO double check the offset
  cv::Mat resized = imageFloat(cv::Rect(46, 13, 227, 227));
  cv::Mat normalized;
  cv::subtract(resized, cv::Scalar(123.68, 116.779, 103.939), normalized);
  cv::Mat reordered;
  cv::cvtColor(normalized, reordered, CV_RGB2BGR);
  */

  /* This operation will write the separate BGR planes directly to the
   * input layer of the network because it is wrapped by the cv::Mat
   * objects in input_channels. */
  //cv::split(normalized, input_channels);

  std::chrono::time_point<std::chrono::steady_clock> netprepTime = std::chrono::steady_clock::now();

  // Keep probability
  tensorflow::Tensor keepProb(tensorflow::DT_FLOAT, tensorflow::TensorShape({1}));
  keepProb.vec<float>()(0) = 1.0f;

  const std::vector<std::pair<std::string, tensorflow::Tensor>> inputs = {{"Placeholder:0", x}, {"Placeholder_2", keepProb}};
  const std::vector<std::string> outputNames = {"ArgMax:0"};
  const std::vector<std::string> nodeNames = {};
  std::vector<tensorflow::Tensor> outputs;

  std::cout << "Run pred" << std::endl;

  tensorflow::Status s = bundle.session->Run(inputs, outputNames, nodeNames, &outputs);
  std::cout << s.ToString() << std::endl;

  std::cout << outputs[0].DebugString() << std::endl;

  std::cout << "Post pred" << std::endl;

  return EXIT_SUCCESS;
}
