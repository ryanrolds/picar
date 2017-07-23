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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define MAXBUF 65536
#define BROADCASTPORT 7492
#define SERVERPORT 7493
bool quit = false;
unsigned sinlen = sizeof(struct sockaddr_in);  
const bool DEBUG = true;

std::vector<std::thread> brains;

int discoveryHost();
int brainHost(int);
void writeFrame(char*, unsigned int, unsigned int, unsigned int, int);

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
  std::cout << "Sock port: " << htons(sock_in.sin_port) << std::endl;

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
    std::cout << "Brain sock port " << htons(brain_sock_in.sin_port) << std::endl;

    // Create thread and add to vector
    brains.push_back(std::thread(brainHost, brain_sock));

    // TODO zombie thread handling/cleanup
    
    // Send new host port to client
    status = sendto(sock, (struct sockaddr *)&brain_sock_in.sin_port, sizeof(brain_sock_in.sin_port), 0, (struct sockaddr *)&sock_in, sinlen);
  }
  
  shutdown(sock, 2);
  close(sock);
}

int brainHost(int sock) {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sinlen); 
  
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
    std::cout << "tick" << std::endl;
    
    // Read 3 bytes
    int bytes = read(client, buffer, 3);
    if (bytes < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
    }

    std::cout << "Recieved status" << std::endl;
    
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
      unsigned int bytes = read(client, buffer, MAXBUF);
      if (bytes < 0) {
	std::cout << "Error reading frame " << bytes << std::endl;
	throw std::runtime_error("Error: " + std::string(strerror(errno)));
      }
    
      // Write data to file
      //fwrite(buffer, sizeof(char), bytes, networkFile);
      
      // Write data to frame buffer
      memcpy(frame + pos, buffer, bytes);
      pos += bytes;

      std::cout << "Getting frame" << std::dec << pos << std::endl;
    }

    //fclose(networkFile);
    
    std::cout << "Received frame " << pos << std::endl;

    // Write collected frame to disk
    if (counter % 20 == 0) {
      writeFrame(frame, frameSize, frameWidth, frameHeight, frameCounter);
      frameCounter++;
    }

    if (obstacle > 0) {
      buffer[0] = 1; // Left	
      buffer[1] = 1; // Back

      obstacle -= 1;
    } else if (unstuck > 0) {
      buffer[0] = 128; // Left	
      buffer[1] = 1; // Back

      unstuck -= 1;      
    } else {
      buffer[0] = 0; // Center	
      buffer[1] = 128; // Forward 
    } 
    
    // Send command   
    int status = send(client, buffer, 2, 0);
    if (status < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
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

void writeFrame(char* frame, unsigned int frameSize, unsigned int frameWidth, unsigned int frameHeight, int counter) {
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
  sprintf(buffer, "P6\n%d %d %d\n", frameWidth, frameHeight, 255);
  // Header
  fwrite(buffer, sizeof(char), strlen(buffer), frameFile);
  // Data
  fwrite(frame, sizeof(char), frameSize, frameFile);
  fclose(frameFile);
}
