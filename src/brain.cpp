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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <caffe/caffe.hpp>
#include <opencv2/opencv.hpp>

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
int setupCaffe();
std::shared_ptr<caffe::Net<float>> setupNet();
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

std::shared_ptr<caffe::Net<float>> setupNet() {
  caffe::Caffe::set_mode(caffe::Caffe::CPU);

  std::shared_ptr<caffe::Net<float>> net;

  /* Load the network. */
  net.reset(new caffe::Net<float>(model_file, caffe::TEST));
  net->CopyTrainedLayersFrom(trained_file);

  //CHECK_EQ(net->num_inputs(), 1) << "Network should have exactly one input.";
  //CHECK_EQ(net->num_outputs(), 1) << "Network should have exactly one output.";

  // Prep net
  caffe::Blob<float>* input_layer = net->input_blobs()[0];
  input_layer->Reshape(1, input_layer->channels(), input_layer->height(), input_layer->width());
  net->Reshape();

  return net;
}

cv::Mat setupMean(caffe::Blob<float>* input_layer) {
    // Mean file
  caffe::BlobProto blob_proto;
  ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);

  /* Convert from BlobProto to Blob<float> */
  caffe::Blob<float> mean_blob;
  mean_blob.FromProto(blob_proto);

  /* The format of the mean file is planar 32-bit float BGR or grayscale. */
  std::vector<cv::Mat> channels;

  float* data = mean_blob.mutable_cpu_data();
  for (int i = 0; i < input_layer->channels(); ++i) {
    /* Extract an individual channel. */
    cv::Mat channel(mean_blob.height(), mean_blob.width(), CV_32FC1, data);
    channels.push_back(channel);
    data += mean_blob.height() * mean_blob.width();
  }

  /* Merge the separate channels into a single image. */
  cv::Mat mean;
  cv::merge(channels, mean);

  /* Compute the global mean pixel value and create a mean image
   * filled with this value. */
  cv::Scalar channel_mean = cv::mean(mean);

  //std::cout << mean.cols << " " << mean.rows << std::endl;
  //std::cout << input_layer->width() << " " << input_layer->height() << std::endl;

  return cv::Mat(cv::Size(input_layer->width(), input_layer->height()), mean.type(), channel_mean);
}

void setupInputLayer(caffe::Blob<float>* input_layer, std::vector<cv::Mat>* input_channels) {
  int width = input_layer->width();
  int height = input_layer->height();
  float* input_data = input_layer->mutable_cpu_data();
  for (int i = 0; i < input_layer->channels(); ++i) {
    cv::Mat channel(height, width, CV_32FC1, input_data);
    input_channels->push_back(channel);
    input_data += width * height;
  }
}

int brainHost(int sock) {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sinlen);

  std::cout << "Preparing net..." << std::endl;

  // Setup CNN and other classification objects
  std::shared_ptr<caffe::Net<float>> net = setupNet();
  caffe::Blob<float>* input_layer = net->input_blobs()[0];
  cv::Mat mean = setupMean(input_layer);
  std::vector<cv::Mat> input_channels;
  setupInputLayer(input_layer, &input_channels);

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
  int unstuckCounter = 0;

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
      unstuckCounter++;
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

      cv::Mat resized = imageFloat(cv::Rect(46, 6, 227, 227));
      cv::Mat normalized;
      cv::subtract(resized, mean, normalized);

      //std::cout << normalized.cols << std::endl;
      //std::cout << normalized.rows << std::endl;

      cv::cvtColor(normalized, normalized, CV_RGB2BGR);

      /* This operation will write the separate BGR planes directly to the
       * input layer of the network because it is wrapped by the cv::Mat
       * objects in input_channels. */
      cv::split(normalized, input_channels);

      net->Forward();

      // Get predictions
      caffe::Blob<float>* output_layer = net->output_blobs()[0];
      const float* begin = output_layer->cpu_data();
      const float* end = begin + output_layer->channels();
      std::vector<float> predictions(begin, end);

      /* Print the top N predictions. */
      float best = 0.0f;
      int bestLabel = 0;
      for (size_t i = 0; i < predictions.size(); ++i) {
	if (predictions[i] > best) {
	  best = predictions[i];
	  bestLabel = i;
	}

	//std::cout << i << ": " << std::fixed << std::setprecision(4) << predictions[i] << std::endl;
      }

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

	// Every few unstuck cycles move forward instead
	if (unstuckCounter % 3 == 0) {
	  buffer[1] = 128; // Forward
	} else {
	  buffer[1] = 1; // Back
	}
	
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
