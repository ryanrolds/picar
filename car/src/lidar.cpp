#include <stdexcept>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <list>

#include <bcm2835.h>

#define BUFFER_SIZE 4096
#define CS_PIN 18

unsigned char buffer[BUFFER_SIZE];
unsigned int channel = 0;

struct DetectionInfo {
  uint32_t timestamp;
  uint16_t num_detections;
  uint16_t power_perc;
  uint32_t options;
};

struct Detection {
  uint32_t distance;
  uint32_t raw;
  uint16_t segment;
  uint16_t flags;
};

int main(int argc, char *argv[]) {
  printf("Starting\n");

  int result = bcm2835_init();
  if (!result) {
    throw std::runtime_error("bcm2835_init failed");
  }

  result = bcm2835_spi_begin();
  if (!result) {
    throw std::runtime_error("bcm2835_spi_begin failed");
  }

  bcm2835_spi_transfer_active();

  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

  unsigned char buffer[BUFFER_SIZE];
  bzero(buffer, BUFFER_SIZE);

  /*
  bcm2835_spi_send(0x05);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x00);
  int len = 3;
  */
 
  bcm2835_spi_send(0x0B);
  bcm2835_spi_send(0x50);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x00);
  bcm2835_spi_send(0x04);
  bcm2835_spi_send(0x00);
  int len = 1026;

  usleep(1000);

  for(int i = 0; i < len; i++) {
    uint8_t recvd = bcm2835_spi_send(0x00);
    printf("0x%02x ", recvd);
    buffer[i] = recvd;
  }

  bcm2835_spi_transfer_end();
 
  // Shutdown  
  bcm2835_spi_end();
  bcm2835_close();

  DetectionInfo info;
  memcpy(&info, buffer, sizeof(DetectionInfo));

  std::list<Detection> detections;

  printf("uptime %d\n", info.timestamp);
  printf("detections %d\n\n", info.num_detections);

  unsigned int offset = sizeof(DetectionInfo);
  for(int i = 0; i < info.num_detections; i++) {
    Detection detection;
    memcpy(&detection, buffer + offset, sizeof(Detection));
    detections.push_back(detection);

    offset += sizeof(Detection); 

    printf("Segment %d\n", detection.segment);
    printf("Distance %d\n", detection.distance);
    printf("Flags %04x\n\n", detection.flags);
  }

  

  return 1;	
}
