#include "brain.hpp"

#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

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


std::vector<std::thread> brains;

int discoveryHost();
int brainHost(int);

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

    std::cout << "Recv: " <<  buffer << std::endl;
    
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
    // TODO zombie thread handling/cleanup
    brains.push_back(std::thread(brainHost, brain_sock));

    // Send new host port to client
    status = sendto(sock, (struct sockaddr *)&brain_sock_in.sin_port, sizeof(brain_sock_in.sin_port),
		    0, (struct sockaddr *)&sock_in, sinlen);
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
  while (true) {
    // Read
    memset(buffer, 0, MAXBUF);
    int bytes = read(client, buffer, MAXBUF);
    if (bytes < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
    }

    std::cout << "Recieved: " << buffer << std::endl;
    
    // Process
    sleep(5);
    
    // Acknowledge
    sprintf(buffer, "cont");
    int buflen = strlen(buffer);
    int status = send(client, buffer, MAXBUF, 0);
    if (status < 0) {
      throw std::runtime_error("Error: " + std::string(strerror(errno)));
    }
  }

  // Done
};
