/**
 * Picar
 * Author: Ryan R. Olds
 *
 * Discovers remote brain service and begins sending camera frames. Network protocol
 * is straight forward. Car sends a video frames and waits for command from brain.
 * Commands control car movement as well as program exit.
 */
#include "car.hpp"
#include "control.hpp"
#include "senses.hpp"
#include <raspicam/raspicam.h>

#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <exception>
#include <system_error>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdlib>

#define MAXBUF 65536
#define BROADCASTIP "192.168.1.255"
#define BROADCASTPORT 7492
#define HOSTPORT 7493
#define COMMANDPORT 7494

const bool DEBUG = false;
const unsigned int CAPTURE_WIDTH = 320;
const unsigned int CAPTURE_HEIGHT = 240;

struct sockaddr_in discoverHost();
int connectToHost(sockaddr_in*, char*);
void handshake(int, char*, int, unsigned int, unsigned int);
void prepareCamera(raspicam::RaspiCam&);
void theLoop(int, char*, raspicam::RaspiCam&, int);

bool running = true;
unsigned sinlen = sizeof(struct sockaddr_in);

static void sigint_catch(int signo) {
  std::cout << "Caught " << signo << std::endl;
  running = false;
}

int main(int argc, const char** argv) {
  // Use a single buffer
  char buffer[MAXBUF];
  memset(buffer, 0, MAXBUF);

  struct sockaddr_in server;
  int control = 0;
  try {
    raspicam::RaspiCam Camera;
    // Prepare camera
    prepareCamera(Camera);
    control = setupControl();
    setupSenses();
    
    int frameSize = Camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_RGB);

    std::cout << "Frame size: " << frameSize << std::endl;
    std::cout << "Frame dim: " << Camera.getWidth() << "x" << Camera.getHeight() << std::endl;

    // Discovery host
    server = discoverHost();

    // Connect to host
    int sock = connectToHost(&server, buffer);

    // Handshake with host
    handshake(sock, buffer, frameSize, Camera.getWidth(), Camera.getHeight());

    // Run the loop
    theLoop(sock, buffer, Camera, control);

    std::cout << "Exiting" << std::endl;
  } catch(std::runtime_error e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (std::exception e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

struct sockaddr_in discoverHost() {
  std::cout << "Discovering host..." << std::endl;

  // Create UDP socket
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  struct sockaddr_in sock_in;
  memset(&sock_in, 0, sinlen);

  // All interfaces and get a random port
  sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_in.sin_port = htons(0);
  sock_in.sin_family = PF_INET;

  // Bind socket
  int status = bind(sock, (struct sockaddr *)&sock_in, sinlen);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  status = getsockname(sock, (struct sockaddr *)&sock_in, &sinlen);
  std::cout << "Sock port " <<  htons(sock_in.sin_port) << std::endl;

  int recvPort = sock_in.sin_port;

  int yes = 1;
  status = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int));
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  // Prep broadcast udp socket
  const char *ip = BROADCASTIP;
  sock_in.sin_addr.s_addr = inet_addr(ip); // Change this to network brodcast
  sock_in.sin_port = htons(BROADCASTPORT);
  sock_in.sin_family = PF_INET;

  char buffer[MAXBUF];
  sprintf(buffer, "marco");
  int buflen = strlen(buffer);
  status = sendto(sock, buffer, buflen, 0, (struct sockaddr *)&sock_in, sinlen);
  unsigned short host_port;
  memset(&host_port, 0, sizeof(host_port));

  int tries = 0;
  while(true) {
    if (tries > 4) {
      throw std::runtime_error("Error: discovery failed");
    }

    std::cout << "Attempting discovery" << std::endl;

    struct timeval read_timeout;
    read_timeout.tv_sec = 5;
    read_timeout.tv_usec = 0;
    status = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    // Recieve new broadcasts
    status = recvfrom(sock, &host_port, sizeof(host_port), 0, (struct sockaddr *)&sock_in, &sinlen);
    if (status == -1) {
      if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
	throw std::runtime_error("Error: " + std::string(strerror(errno)));
      }

      std::cout << "Discovery timedout" << std::endl;

      // Try again
      tries++;
      continue;
    }

    break; // Discovery successful
  }

  shutdown(sock, 2);
  close(sock);

  std::cout << "Got response from " << inet_ntoa(sock_in.sin_addr) << ":" << htons(sock_in.sin_port) << std::endl;

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr = sock_in.sin_addr;
  server.sin_port = host_port;

  return server;
}

int connectToHost(sockaddr_in* server, char* buffer) {
  std::cout << "Connecting... " << inet_ntoa(server->sin_addr) << ":" << htons(server->sin_port) << std::endl;

  usleep(5); // Not happy with this, shrug

  // Setup connection brain
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  // Bind socket
  int status = connect(sock, (struct sockaddr*) server, sinlen);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  return sock;
}

void handshake(int sock, char* buffer, int frameSize, unsigned int width, unsigned int height) {
  std::cout << "Connected" << std::endl;

  // Sent client version
  sprintf(buffer, "v0.0.1");
  int status = send(sock, buffer, strlen(buffer), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  std::cout << "Sent client version" << std::endl;

  // Prepare to receive to
  memset(buffer, 0, MAXBUF);

  // Recieve server version
  status = recv(sock, buffer, MAXBUF, 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  std::cout << "Server version: " << buffer << std::endl;

  if (strcmp(buffer, "v0.0.1") != 0) {
    // TODO handle version mismatch
    throw std::runtime_error("Error: Unsupported server version" + std::string(buffer));
  }
  
  // Sent frame size
  int networkFrameSize = htonl(frameSize);
  status = send(sock, &networkFrameSize, sizeof(networkFrameSize), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  unsigned int networkWidth = htonl(width);
  status = send(sock, &networkWidth, sizeof(networkWidth), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  unsigned int networkHeight = htonl(height);
  status = send(sock, &networkHeight, sizeof(networkHeight), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }
  
  std::cout << "Handshake complete" << std::endl;
}

void prepareCamera(raspicam::RaspiCam &Camera) {
  std::cout << "Camera processing starting" << std::endl;

  Camera.setCaptureSize(CAPTURE_WIDTH, CAPTURE_HEIGHT);
  Camera.setVerticalFlip(true);
  Camera.setHorizontalFlip(true);
  Camera.setExposure(raspicam::RASPICAM_EXPOSURE_AUTO); // Max 30
  Camera.setFormat(raspicam::RASPICAM_FORMAT_RGB);
  
  if (!Camera.open()) {
    throw std::runtime_error("Error: Unable to open camera");
  }
  
  std::cout << "Camera: " << Camera.getId() << std::endl;
}

void theLoop(int sock, char* buffer, raspicam::RaspiCam &Camera, int control) {
  // Prepare signal handler, will be used to abort the loop
  if (signal(SIGINT, sigint_catch) == SIG_ERR) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  int status;
  int frameSize = Camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_RGB);
  unsigned char *frame = new unsigned char[frameSize];

  std::cout << "Starting loop" << std::endl;

  int counter = 0;
  
  while(true) {
    //std::cout << "tick" << std::endl;

    if (running) {
      buffer[0] = 1;
    } else {
      std::cout << std::endl << "Sending shutdown to brain" << std::endl;
      buffer[0] = 0;
    }
    
    buffer[1] = hasObstacle() ? 1 : 0; // Obstical sensor
    
    status = send(sock, buffer, 2, 0);    
    if (status < 0) {
      break;
      // TODO handle error
    }

    //std::cout << "Sent running and obs sensor" << std::endl;

    if (!running) {
      break;
    }

    // Get frame
    Camera.grab();
    Camera.retrieve(frame);

    //std::cout << "Frame size: " << sizeof(frame) << std::endl;    
    
    // Send sensor to brain
    status = send(sock, frame, frameSize, 0);
    if (status < 0) {
      throw std::runtime_error("Error sending frame" + std::string(strerror(errno)));
    }

    if (status != frameSize) {
      std::cout << "Sent bytes does not match framesize " + status << std::endl;
    }

    //std::cout << "Sent frame" << std::endl;
    
    // Read control data    
    status = recv(sock, buffer, 2, 0);
    if (status < 0) {
      break;
      // TODO handle error
    }

    //std::cout << "Recieved: " << (int)buffer[0] << " " << (int)buffer[1] << std::endl;
    
    // Steering
    if (buffer[0] == 0) {
      //std::cout << "cent" << std::endl;      
      setSteering(control, STEERING_CEN);
    } else if (buffer[0] > 127) {
      //std::cout << "right" << std::endl;	    	    
      setSteering(control, STEERING_MAX - 30);
    } else if (buffer[0] < 128) {
      //std::cout << "left" << std::endl;	    	    
      setSteering(control, STEERING_MIN + 30);
    }
    
    // Forward/backward
    if (buffer[1] == 0) { // Stop
      //std::cout << "stop" << std::endl;
      setStop(control);
    } else if (buffer[1] > 127) { // Forward
      //std::cout << "fore" << std::endl;	    
      setForward(control, 3700);
    } else if (buffer[1] < 128) { // Backward
      //std::cout << "back" << std::endl;	    
      setBackward(control, 4000);
    }
    
    if (DEBUG) {
      std::cout << "tock" << std::endl;
    } else if (counter % 10 == 0) {
      std::cout << "." << std::flush;
    }
    
    counter++;
  }

  setSteering(control, STEERING_CEN);
  setStop(control);
  
  std::cout << "Loop complete" << std::endl;
}
