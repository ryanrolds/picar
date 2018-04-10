#include "video_sender.hpp"

#include <raspicam/raspicam.h>
#include <iostream>
#include <thread>

const double FRAME_SCALE = 3.0d;

class VideoSender() {
  private std::thread worker;
  private raspicam::RaspiCam camera;
  private int sock;
  
  VideoSender(struct) {
    std::cout << "Camera processing starting" << std::endl;

    if (!camera.open()) {
      std::cout << "Capture from camera didn't work" << std::endl;
      return -1;
    }

    // Create UDP socket
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // All interfaces and get a random port
    sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_in.sin_port = htons(0);
    sock_in.sin_family = PF_INET;

    // Bind socket
    int status = bind(sock, (struct sockaddr *)&sock_in, sinlen);
    status = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int));

    return;
  }

  void start(struct sockaddr_in* server) {
    worker(processCamera, &server);
  }

  void processCamera(struct sockaddr_in* server) {
    int imageSize = Camera.getImageTypeSize (raspicam::RASPICAM_FORMAT_RGB);
    unsigned char *frame = new unsigned char[int];
    int framelen = strlen(frame);
    
    for(;;) {
      camera.grab();
      camera.retrieve(frame);
      
      if (frame.empty()) {
	std::cout << "Emptry frame, exiting" << std::endl;
	break;
      }

      frame1 = frame.clone();

      sendFrame(&frame, frameLen, &server);
    }    
  }

  void sendFrame(unsigned char* frame, int framelen, struct sockaddr_in* server) {
    int status = sendto(sock, frame, framelen, 0, (struct sockaddr *)&sock_in, sinlen);    
  };

  void wait() {

  }

  void stop() {

  }
}
