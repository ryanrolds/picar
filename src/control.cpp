#include "control.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>

#define REAR_L_FORWARD 17
#define REAR_L_BACKWARD 27
#define REAR_R_FORWARD 23
#define REAR_R_BACKWARD 24

// Registers
__u8 mode1 = 0x00; //settings 1
__u8 mode2 = 0x01; // settings 2
__u8 pulseScale = 0xFE; // Puise scale

int setupControl() {
  int file = open("/dev/i2c-1", O_RDWR);
  if (file < 0) {
    std::cout <<(strerror(errno)) << std::endl;
    return -1;
  }

  char rxBuffer[32];  // receive buffer
  char txBuffer[32];  // transmit buffer

  // Clear our buffers
  memset(rxBuffer, 0, sizeof(rxBuffer));
  memset(txBuffer, 0, sizeof(txBuffer));

  int result; // store i2c results
  
  // Set address for slave
  int addr = 0x40; /* The I2C address */
  result = ioctl(file, I2C_SLAVE, addr);
  if (result < 0) {
    std::cout << "ioctl " << strerror(errno) << std::endl;
    return -1;
  }
 
  __s32 res; // smbus result

  // Read mode1 register
  res = i2c_smbus_read_byte_data(file, 0x00);
  if (res < 0) {
    std::cout << "Read error " << strerror(errno) << std::endl;
    return -1;
  } 
  std::cout << "Read results " << std::hex << res << std::endl;
  
  __u8 newMode = 0x30; // Sleep and autoinc  
  res = i2c_smbus_write_byte_data(file, 0x00, newMode);
  if (res < 0) {
    std::cout << "WRite error " << strerror(errno) << std::endl;
    return -1;
  }
  std::cout << "Write results " << std::hex << res << std::endl;

  // Read mode1 register
  res = i2c_smbus_read_byte_data(file, 0x00);
  if (res < 0) {
    std::cout << "Read error " << strerror(errno) << std::endl;
    return -1;
  } 
  std::cout << "Read results " << std::hex << res << std::endl;
  
  __u8 PRESCALE = 0x79; // 121hz = 25Mhz / (4096 * 50hz)
  
  std::cout << "PWM freq " << std::dec << 50 << " prescale " << PRESCALE << std::endl;
  res = i2c_smbus_write_byte_data(file, 0xFE, PRESCALE);
  if (res < 0) {
    std::cout << "PWM error " << strerror(errno) << std::endl;
    return -1;
  }
  std::cout << "PWM results " << std::hex << res << std::endl;

  newMode = 0x20; // autoinc  
  res = i2c_smbus_write_byte_data(file, 0x00, newMode);
  if (res < 0) {
    std::cout << "WRite error " << strerror(errno) << std::endl;
    return -1;
  }
  std::cout << "Write results " << std::hex << res << std::endl;

  // Read mode1 register
  res = i2c_smbus_read_byte_data(file, 0x00);
  if (res < 0) {
    std::cout << "Read error " << strerror(errno) << std::endl;
    return -1;
  } 
  std::cout << "Read results " << std::hex << res << std::endl;

  // Read mode1 register
  res = i2c_smbus_read_byte_data(file, 0x01);
  if (res < 0) {
    std::cout << "Read error " << strerror(errno) << std::endl;
    return -1;
  } 
  std::cout << "Read results " << std::hex << res << std::endl;

  // Read mode1 register
  res = i2c_smbus_read_byte_data(file, 0xFE);
  if (res < 0) {
    std::cout << "Read error " << strerror(errno) << std::endl;
    return -1;
  } 
  std::cout << "Read results " << std::hex << res << std::endl;

  // Setup Wiring Pi
  result = wiringPiSetupSys();
  if (result == -1) {
    std::cout << "Error with wiring pi" << std::endl;
  }
  
  pinMode(REAR_L_FORWARD, OUTPUT);
  pinMode(REAR_L_BACKWARD, OUTPUT);
  pinMode(REAR_R_FORWARD, OUTPUT);
  pinMode(REAR_R_BACKWARD, OUTPUT);

  // Set default control positions
  setSteering(file, STEERING_CEN);
  setStop(file);
  
  std::cout << "All good!" << std::endl;
  
  return file;
};

void cleanup(int file) {
  __u8 newMode = 0x30; // Sleep and autoinc  
  int res = i2c_smbus_write_byte_data(file, 0x00, newMode);
  if (res < 0) {
    std::cout << "WRite error " << strerror(errno) << std::endl;
  }
  std::cout << "Write results " << std::hex << res << std::endl;
  
  close(file);
};

void setForward(int file, __u16 speed) {
  //std::cout << "Foward " << speed << std::endl;

  __u16 left = speed;
  __u16 right = speed;
  
  digitalWrite(REAR_R_FORWARD, HIGH);
  digitalWrite(REAR_R_BACKWARD, LOW);
  i2c_smbus_write_byte_data(file, 0x16, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x17, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x18, right & 0xFF);
  i2c_smbus_write_byte_data(file, 0x19, right >> 8);

  digitalWrite(REAR_L_FORWARD, HIGH);
  digitalWrite(REAR_L_BACKWARD, LOW);
  i2c_smbus_write_byte_data(file, 0x1A, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1B, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x1C, left & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1D, left >> 8);
};

void setBackward(int file, __u16 speed) {
  //std::cout << "Foward " << speed << std::endl;

  __u16 left = speed;
  __u16 right = speed;
  
  digitalWrite(REAR_R_FORWARD, LOW);
  digitalWrite(REAR_R_BACKWARD, HIGH);
  i2c_smbus_write_byte_data(file, 0x16, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x17, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x18, right & 0xFF);
  i2c_smbus_write_byte_data(file, 0x19, right >> 8);

  digitalWrite(REAR_L_FORWARD, LOW);
  digitalWrite(REAR_L_BACKWARD, HIGH);
  i2c_smbus_write_byte_data(file, 0x1A, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1B, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x1C, left & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1D, left >> 8);
};

void setStop(int file) {
  //std::cout << "Stop" << std::endl;

  digitalWrite(REAR_L_FORWARD, LOW);
  digitalWrite(REAR_L_BACKWARD, LOW);
  digitalWrite(REAR_R_FORWARD, LOW);
  digitalWrite(REAR_R_BACKWARD, LOW);
  
  __u16 speed = 0x00;
  
  i2c_smbus_write_byte_data(file, 0x16, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x17, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x18, speed & 0xFF);
  i2c_smbus_write_byte_data(file, 0x19, speed >> 8);

  i2c_smbus_write_byte_data(file, 0x1A, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1B, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x1C, speed & 0xFF);
  i2c_smbus_write_byte_data(file, 0x1D, speed >> 8);
}

void setSteering(int file, __u16 off) {
  if (off < STEERING_MIN) {
    off = STEERING_MIN;
  } else if (off > STEERING_MAX) {
    off = STEERING_MAX;
  }  

  //std::cout << "Steering " << std::dec << off << std::endl;
  
  i2c_smbus_write_byte_data(file, 0x06, 0x00 & 0xFF);
  i2c_smbus_write_byte_data(file, 0x07, 0x00 >> 8);
  i2c_smbus_write_byte_data(file, 0x08, off & 0xFF);
  i2c_smbus_write_byte_data(file, 0x09, off >> 8);
};
