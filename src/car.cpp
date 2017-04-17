#include "car.hpp"

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

#define MAXBUF 65536
#define BROADCASTIP "192.168.1.255"
#define BROADCASTPORT 7492
#define HOSTPORT 7493
#define COMMANDPORT 7494

//VideoSender video;
//CommandReciver command;

struct sockaddr_in discoverHost();
bool running = true;
unsigned sinlen = sizeof(struct sockaddr_in);

static void sigint_catch(int signo) {
  std::cout << "Caught " << signo << std::endl;
  running = false;
  
  //command.stop();
  //video.stop();
}

int main(int argc, const char** argv) {
  //video = new VideoSender();
  //command = new CommandReciver();
  
  if (signal(SIGINT, sigint_catch) == SIG_ERR) {
    return EXIT_FAILURE;
  }
  
  struct sockaddr_in server;  
  try {
    server = discoverHost();
  } catch (std::exception e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  
  // Setup connection brain
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }
       
  // Bind socket
  int status = connect(sock, (struct sockaddr *)&server, sinlen);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  // Handshake
         
  while (running) {
    // Write frame+sensor to brain

    // Read control data    
  }
  
  // Start video+sensor sender thread
  //video.start(server);

  // Start command receiver thread
  //command.start();

  //command.wait();
  //video.wait();
  
  std::cout << "Exiting" << std::endl;

  return EXIT_SUCCESS;
}

struct sockaddr_in discoverHost() {
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

    memset(buffer, 0, buflen);
  
    // Recieve new broadcasts
    status = recvfrom(sock, buffer, buflen, 0, (struct sockaddr *)&sock_in, &sinlen);
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
  
  std::cout << "Got response from " << inet_ntoa(sock_in.sin_addr) << std::endl;
  std::cout << buffer << std::endl;
  
  return sock_in;
}
