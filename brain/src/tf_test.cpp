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
  std::string testImage = "./frame_2017-07-25T21-00-04Z_788.ppm";
  std::ifstream in(testImage);
  std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()); 
  const char* ppmFile = contents.c_str();

  tensorflow::Tensor image_decode(tensorflow::DT_UINT8, tensorflow::TensorShape({frameHeight, frameWidth, depth}));
  auto image_flat = image_decode.flat<uint8_t>();
  std::copy_n(ppmFile+116, frameHeight * frameWidth * depth, image_flat.data());
  //memset(image_flat.data(), 0, frameHeight * frameWidth * depth);

  std::cout << image_flat.data() << std::endl;

  std::cout << ppmFile[0] << " " << ppmFile[1] << " " << ppmFile[2] << std::endl;
  std::cout << image_decode.DebugString() << std::endl;
  std::cout << image_decode.SummarizeValue(9) << std::endl;

  //std::string testImage = "./frame_2017-07-25T21-00-04Z_788.png";
  //tensorflow::Output image_string = tensorflow::ops::ReadFile(root, testImage);
  //tensorflow::Output image_decode = tensorflow::ops::DecodePng(root, image_string);
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
  session.Run({image_expand}, &image_outputs);

  std::cout << "Post run image" << std::endl;

  tensorflow::Tensor x = image_outputs[0];

  std::cout << "Image" << x.DebugString() << std::endl;
  std::cout << x.SummarizeValue(9) << std::endl;

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
