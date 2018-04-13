#include "brain.hpp"

Brain::Brain() {};

BrainHost Brain::discover() {
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

void Brain::connect(BrainHost &host) {
  std::cout << "Connecting... " << inet_ntoa(host->server->sin_addr) << ":" << htons(host->server->sin_port) << std::endl;

  usleep(5); // Not happy with this, shrug

  // Setup connection brain
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  // Bind socket
  int status = connect(sock, (struct sockaddr*) host->server, sinlen);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  host->socket = sock;
}

void Brain::handshake(BrainHost &host, Frame &frame) {
  std::cout << "Connected" << std::endl;

  char *buffer = char[128];

  // Sent client version
  sprintf(buffer, "v0.0.1");
  int status = send(sock, buffer, strlen(buffer), 0);
  if (status < 0) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  std::cout << "Sent client version" << std::endl;

  // prepare to receive to
  //memset(buffer, 0, maxbuf);

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


void Brain::run(BrainHost &brain, Frame &frame, Command &command) {
  // Prepare signal handler, will be used to abort the loop
  if (signal(SIGINT, sigint_catch) == SIG_ERR) {
    throw std::runtime_error("Error: " + std::string(strerror(errno)));
  }

  int status;
  int counter = 0;

  std::cout << "Starting loop" << std::endl;

  int frameSize = frame.getSize();
  unsigned char* frameBuffer = new unsigned char[frameSize];
  
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

    // TODO get camera frame
    frame.get(frameBuffer);

    //std::cout << "Frame size: " << sizeof(frameBuffer) << std::endl;    
    
    // Send sensor to brain
    status = send(sock, frameBuffer, frameSize, 0);
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
};

void Brain::disconnect(BrainHost brain) {

};
