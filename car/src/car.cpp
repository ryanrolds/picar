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
#include "camera.hpp"
#include "senses.hpp"
#include "frame.hpp"
#include "brain.hpp"

#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <exception>
#include <system_error>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <thread>

#define MAXBUF 65536
#define BROADCASTIP "192.168.1.255"
#define BROADCASTPORT 7492
#define HOSTPORT 7493
#define COMMANDPORT 7494

static void sigint_catch(int signo) {
  std::cout << "Caught " << signo << std::endl;

  // TODO tell threads to shutdown
}

int main(int argc, const char** argv) {
  try {
    // Prepare camera
    CameraHandler camera;
    camera.prepare();
    int frameSize = camera.getFrameSize();

    // Create frame buffer
    Frame frame(frameSize);

    // Start camera thread
    std::thread cameraWorker([](CameraHandler &camera, Frame &frame) {
      camera.run(frame);
    }, camera, frame);

    // Setup command buffer
    Command command;
 
    // Setup connection to brain 
    Brain brain;
    BrainHost host = brain.discover();
    brain.connect(host);
  
    // Start networking threads
    std::thread netWorker([](Brain &brain, Frame &frame, Command &command) {
      brain.run(); 
    }, brain, frame, command);

    // Setup control thread
    control = setupControl();
    setupSenses();
    std::thread controlWorker([](Driver &driver) {
      control.run();
    }, control);

    // Wait for threads to join
    cameraWorker.join();
    netWorker.join();
    controlWorker.join(); 

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

