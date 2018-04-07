#include "brain.hpp"

#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <algorithm>
#include <memory>
#include <tuple>
#include <chrono>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/saved_model/loader.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"

#define MAXBUF 65536
#define BROADCASTPORT 7492
#define SERVERPORT 7493

typedef std::pair<std::string, float> Prediction;

bool quit = false;
unsigned sinlen = sizeof(struct sockaddr_in);
const bool DEBUG = false;

std::string model_file = "./share/net.model";
std::string trained_file = "./share/net.weights";
std::string mean_file = "./share/net.avg";


std::vector<std::thread> brains;

int discoveryHost();
int brainHost(int);
//int setupCaffe();

void setupNet(tensorflow::ClientSession&);
void writeFrame(char*, unsigned int, unsigned int, unsigned int, int, int);

static void sigint_catch(int signo) {
  std::cout << "Caught " << signo << std::endl;
  quit = true;
}

int main(int argc, const char** argv) {
  if (signal(SIGINT, sigint_catch) == SIG_ERR) {
    return EXIT_FAILURE;
  }


  // Start loop that will handle discovery
  int result = discoveryHost();
  // TODO should exit successfully

  std::cout << "Exiting" << std::endl;

  return EXIT_SUCCESS;
}

int discoveryHost() {
  struct sockaddr_in sock_in;
  memset(&sock_in, 0, sinlen);

  struct sockaddr_in sock_out;
  memset(&sock_out, 0, sinlen);

  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_in.sin_port = htons(BROADCASTPORT);
  sock_in.sin_family = PF_INET;

  int status = bind(sock, (struct sockaddr *)&sock_in, sinlen);
  std::cout << "Bind Status: " << status << std::endl;

  status = getsockname(sock, (struct sockaddr *)&sock_in, &sinlen);
  std::cout << "Sock port: " << std::to_string(ntohs(sock_in.sin_port)) << std::endl;

  // Set recvfrom time out to 1 sec
  struct timeval read_timeout;
  read_timeout.tv_sec = 1;
  read_timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

  char buffer[MAXBUF];
  int buflen = MAXBUF;

  while(true) {
    if (quit) {
      std::cout << "quit" << std::endl;
      break;
    }

    memset(buffer, 0, buflen);

    // Recieve new broadcasts
    status = recvfrom(sock, buffer, buflen, 0, (struct sockaddr *)&sock_in, &sinlen);
    if (status == -1) {
      if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
	std::cout << "Error " << errno << std::endl;
	return EXIT_FAILURE;
      } else {
	continue;
      }
    }

    // Setup new socket for client
    struct sockaddr_in brain_sock_in;
    memset(&brain_sock_in, 0, sinlen);

    int brain_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    brain_sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
    brain_sock_in.sin_port = 0;
    brain_sock_in.sin_family = AF_INET;

    int status = bind(brain_sock, (struct sockaddr *)&brain_sock_in, sinlen);
    std::cout << "Brain bind status " << status << std::endl;

    listen(brain_sock, 0);

    status = getsockname(brain_sock, (struct sockaddr *)&brain_sock_in, &sinlen);
    std::cout << "Brain sock port " << std::to_string(ntohs(brain_sock_in.sin_port)) << std::endl;

    // Create thread and add to vector
    brains.push_back(std::thread(brainHost, brain_sock));

    // TODO zombie thread handling/cleanup

    // Send new host port to client
    status = sendto(sock, (struct sockaddr *)&brain_sock_in.sin_port, sizeof(brain_sock_in.sin_port), 0, (struct sockaddr *)&sock_in, sinlen);
  }

  shutdown(sock, 2);
  close(sock);
}

void setupNet(tensorflow::SavedModelBundle& bundle) {
  // Setup graph
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

  //tensorflow::LoadGraph("./graph.pb", session)

  /*
  tensorflow::Session* session;
  tensorflow::Status status = tensorflow::NewSession(tensorflow::SessionOptions(), &session);
  if (!status.ok()) {
    std::cout << status.ToString() << std::endl;
    throw std::runtime_error("Error: " + status.ToString());
  }

  tensorflow::GraphDef graphDef;
  status = tensorflow::ReadBinaryProto(tensorflow::Env::Default(), "", &graphDef);
  if (!status.ok()) {
    std::cout << status.ToString() << std::endl;
    throw std::runtime_error("Error: " + status.ToString());
  }

  return session;
  */
}

int brainHost(int sock) {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sinlen);

  std::cout << "Preparing net..." << std::endl;

  // Setup CNN and other classification objects
  tensorflow::SavedModelBundle bundle;
  setupNet(bundle);

  //caffe::Blob<float>* input_layer = net->input_blobs()[0];
  //cv::Mat mean = setupMean(input_layer);
  std::vector<cv::Mat> input_channels;
  //setupInputLayer(input_layer, &input_channels);

  std::cout << "Accepting connection..." << std::endl;

  int client = accept(sock, (struct sockaddr *)&client_addr, &sinlen);
  if (client < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  char buffer[MAXBUF];
  memset(buffer, 0, MAXBUF);

  // Handshake
  int bytes = read(client, buffer, MAXBUF);
  if (bytes < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  std::cout << "Client version: " <<  buffer << std::endl;

  if (strcmp(buffer, "v0.0.1") != 0) {
    // TODO handle version mismatch
    throw std::runtime_error("Error: Unsupported client version" + std::string(buffer));
  }

  // Write server version
  sprintf(buffer, "v0.0.1");

  int status = send(client, buffer, strlen(buffer), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  std::cout << "Sent client version" << std::endl;

  // Get frame size
  unsigned int frameSize = 0;
  bytes = read(client, &frameSize, sizeof(unsigned int));
  if (bytes < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  frameSize = ntohl(frameSize);

  unsigned int frameWidth = 0;
  bytes = read(client, &frameWidth, sizeof(unsigned int));
  if (bytes < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  frameWidth = ntohl(frameWidth);

  unsigned int frameHeight = 0;
  bytes = read(client, &frameHeight, sizeof(unsigned int));
  if (bytes < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  frameHeight = ntohl(frameHeight);

  printf("Frame size %d %dx%d\n", frameSize, frameWidth, frameHeight);

  std::cout << "Handshake complete" << std::endl;

  char* frame = new char[frameSize];
  memset(frame, 0, frameSize);

  int counter = 0;
  int frameCounter = 0;
  int obstacle = 0;
  int unstuck = 0;

  while (true) {
    //std::cout << "tick" << std::endl;

    // Read 3 bytes
    int bytes = read(client, buffer, 3);
    if (bytes < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
    }

    //std::cout << "Recieved status" << std::endl;

    if (buffer[0] == 0) { // Exit
      std::cout << std::endl << "Car shutting down" << std::endl;
      break;
    }

    if (buffer[1] > 0) { // Obsticle sensor
      obstacle = 10;
    } else if (counter % 100 == 0) {
      unstuck = 10;
    }

    unsigned int pos = 0;

    // Write complete frame to disk
    //char filename[] = "network.ppm";
    //FILE* networkFile = fopen(filename, "w");
    //sprintf(buffer, "P6\n1280 960 255\n");
    // Header
    //fwrite(buffer, sizeof(char), strlen(buffer), networkFile);

    memset(buffer, 0, MAXBUF);

    while(pos < frameSize - 1) {
      int bytes = read(client, buffer, MAXBUF);
      if (bytes < 0) {
	std::cout << "Error reading frame " << bytes << std::endl;
	throw std::runtime_error("Error: " + std::string(strerror(errno)));
      }

      // Write data to file
      //fwrite(buffer, sizeof(char), bytes, networkFile);

      // Write data to frame buffer
      memcpy(frame + pos, buffer, bytes);
      pos += bytes;

      //std::cout << "Getting frame" << std::dec << pos << std::endl;
    }

    //fclose(networkFile);

    buffer[0] = 0; // Center
    buffer[1] = 128; // Forward

    if (pos < frameSize) {
      std::cout << "Wrong frame size " << pos << std::endl;
    } else {


      cv::Mat image = cv::Mat(frameHeight, frameWidth, CV_8UC3, frame);
      cv::Mat imageFloat;
      image.convertTo(imageFloat, CV_32FC3);
      // TODO double check the offset
      cv::Mat resized = imageFloat(cv::Rect(46, 6, 227, 227));

      cv::Mat normalized;
      cv::subtract(resized, mean, normalized);

      std::cout << normalized.cols << std::endl;
      std::cout << normalized.rows << std::endl;

      cv::cvtColor(normalized, normalized, CV_RGB2BGR);

      /* This operation will write the separate BGR planes directly to the
       * input layer of the network because it is wrapped by the cv::Mat
       * objects in input_channels. */
      cv::split(normalized, input_channels);

      std::chrono::time_point<std::chrono::steady_clock> netprepTime = std::chrono::steady_clock::now();


      auto x(tensorflow::DT_FLOAT, tensorflow::TensorShape({1, 227, 227, 3}));
      auto x_mapped = x.tensor<float, 4>();

      // TODO iterator input_channels and write to x_mapped

      const std::vector<std::pair<std::string, tensorflow::Tensor>> inputs = {{"x", x}};
      const std::vector<std::string> outputNames = {"score"};
      const std::vector<std::string> nodeNames = {"score"};
      std::vector<tensorflow::Tensor> outputs;
      tensorflow::Status s = bundle.session->Run(inputs, outputNames, nodeNames, &outputs);

      //net->Forward();

      // Get predictions
      //caffe::Blob<float>* output_layer = net->output_blobs()[0];
      //const float* begin = output_layer->cpu_data();
      //const float* end = begin + output_layer->channels();
      //std::vector<float> predictions(begin, end);

      /* Print the top N predictions. */
      float best = 0.0f;
      int bestLabel = 0;
      /*
      for (size_t i = 0; i < predictions.size(); ++i) {
      	if (predictions[i] > best) {
      	  best = predictions[i];
      	  bestLabel = i;
	      }

	      //std::cout << i << ": " << std::fixed << std::setprecision(4) << predictions[i] << std::endl;
      }
      */

      // std::cout << "Best: " << bestLabel << " " << std::fixed << std::setprecision(4) << best << std::endl;

      switch(bestLabel) {
      case 0:
      case 1:
      case 7:
      	buffer[0] = 0; // Center
	      buffer[1] = 1; // Back
	      break;
	    //case 1:
	      //buffer[0] = 128; // Left
	      //buffer[1] = 1; // Back
	      //break;
      case 2:
	      buffer[0] = -127; // Right
	      buffer[1] = 128; // Forward
	      break;
      case 3:
	      buffer[0] = -63; // Slight right
	      buffer[1] = 128; // Forward
	      break;
      case 4:
      	buffer[0] = 0; // Center
      	buffer[1] = 128; // Forward
      	break;
      case 5:
      	buffer[0] = 64; // Slight Left
      	buffer[1] = 128; // Forward
      	break;
      case 6:
      	buffer[0] = 128; // Left
      	buffer[1] = 128; // Forward
      	break;
    	//case 7:
      	//buffer[0] = -127; // Right
      	//buffer[1] = 1; // Backward
      	//break;
      case 8:
      	buffer[0] = 0; // Center
      	buffer[1] = 1; // Back
      	break;
      default:
      	throw std::runtime_error("Invalid class");
      }
      
      if (obstacle > 0) {
      	buffer[0] = 1; // Left
      	buffer[1] = 1; // Back
	
      	obstacle -= 1;	  
      } else if (unstuck > 0) {
      	buffer[0] = 128; // Left
      	buffer[1] = 1; // Back
	
      	unstuck -= 1;
      }

      if (counter < 5) {
      	buffer[0] = 0;
      	buffer[1] = 128;
      } else if (counter < 10) {
      	buffer[0] = 0;
      	buffer[1] = 1;
      } else if (counter < 15) {
      	buffer[0] = 128;
      	buffer[1] = 0;
      } else if (counter < 20) {
      	buffer[0] = 64;
      	buffer[1] = 0;
      } else if (counter < 25) {
      	buffer[0] = 0;
      	buffer[1] = 0;
      } else if (counter < 30) {
      	buffer[0] = -63;
      	buffer[1] = 0;
      } else if (counter < 35) {
      	buffer[0] = -127;
      	buffer[1] = 0;
      }
    }
    
    // Send command
    int status = send(client, buffer, 2, 0);
    if (status < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
    }

    // Write collected frame to disk
    if (counter % 20 == 0 || obstacle > 0) {
      writeFrame(frame, frameSize, frameWidth, frameHeight, frameCounter, obstacle);
      frameCounter++;
    }

    if (DEBUG) {
      std::cout << "tock" << std::endl;
    } else if (counter % 10 == 0) {
      std::cout << "." << std::flush;
    }

    counter++;
  }

  std::cout << "Brain loop complete" << std::endl;
};

void writeFrame(char* frame, unsigned int frameSize, unsigned int frameWidth, unsigned int frameHeight, int counter, int obstacle) {
  char buffer[MAXBUF];
  //memset(buffer, 0, MAXBUF);

  time_t now;
  time(&now);
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));

  // Write complete frame to disk
  std::string frameFilename;
  frameFilename += "/home/ryan/frames/frame_";
  frameFilename += buf;
  frameFilename += "_" + std::to_string(counter);
  frameFilename += ".ppm";

  std::replace( frameFilename.begin(), frameFilename.end(), ':', '-');

  FILE* frameFile = fopen(frameFilename.c_str(), "w");
  sprintf(buffer, "P6\n%d %d %d\n#ObsSensor:%d\n", frameWidth, frameHeight, 255, obstacle);
  // Header
  fwrite(buffer, sizeof(char), strlen(buffer), frameFile);
  // Data
  fwrite(frame, sizeof(char), frameSize, frameFile);
  fclose(frameFile);
}
