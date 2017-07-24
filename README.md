# Cat Chasing Pi Car

Raspberry Pi + OpenCV driven car. Uses cascades trainned in cat detection to follow a house cat.

## Parts

* Raspberry Pi 3
* Raspberry Pi camera
* Smart Car
* Camera mount (see models)
* IR Sensor mount (see models)

## Status

At this time 

### Complete

* Setup VM and write UDP discovery service
* Image+sensor processing server (VM)
* Image+sensor sender and discovery client (car)
* Move behavior logic and image processing to amd64 VM
* Write image collector and write to NAS
* Write image classifier (wheel and speed)
* Integrate IR Obst. Sensor (car)

### TODO

* Get 10k frames classified
* Train NN/RNN using classified image data
* Create behavior tree (car)

## Setup

Dependencies: 

* https://sourceforge.net/projects/raspicam/files/
* http://opencv.org/ (3.1.0)
* http://wiringpi.com/download-and-install/

## Build

### cmake

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make  

### Old

    $ g++ -std=c++11 -I include/ src/main.cpp src/detect.cpp src/control.cpp -o bin/car -lraspicam -lraspicam_cv `pkg-config --cflags --libs opencv` -pthread -lwiringPi

## GPIO Setup

Program uses i2c (2, 3) and OUPUT on 17, 27, 23, 24

    $ gpio readal
    +-----+-----+---------+------+---+---Pi 3---+---+------+---------+-----+-----+
    | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
    +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
    |     |     |    3.3v |      |   |  1 || 2  |   |      | 5v      |     |     |
    |   2 |   8 |   SDA.1 | ALT0 | 1 |  3 || 4  |   |      | 5V      |     |     |
    |   3 |   9 |   SCL.1 | ALT0 | 1 |  5 || 6  |   |      | 0v      |     |     |
    |   4 |   7 | GPIO. 7 |   IN | 1 |  7 || 8  | 0 | IN   | TxD     | 15  | 14  |
    |     |     |      0v |      |   |  9 || 10 | 1 | IN   | RxD     | 16  | 15  |
    |  17 |   0 | GPIO. 0 |  OUT | 0 | 11 || 12 | 0 | ALT0 | GPIO. 1 | 1   | 18  |
    |  27 |   2 | GPIO. 2 |  OUT | 0 | 13 || 14 |   |      | 0v      |     |     |
    |  22 |   3 | GPIO. 3 |   IN | 0 | 15 || 16 | 0 | OUT  | GPIO. 4 | 4   | 23  |
    |     |     |    3.3v |      |   | 17 || 18 | 0 | OUT  | GPIO. 5 | 5   | 24  |
    |  10 |  12 |    MOSI |   IN | 0 | 19 || 20 |   |      | 0v      |     |     |
    |   9 |  13 |    MISO |   IN | 0 | 21 || 22 | 0 | IN   | GPIO. 6 | 6   | 25  |
    |  11 |  14 |    SCLK |   IN | 0 | 23 || 24 | 1 | IN   | CE0     | 10  | 8   |
    |     |     |      0v |      |   | 25 || 26 | 1 | IN   | CE1     | 11  | 7   |
    |   0 |  30 |   SDA.0 |   IN | 1 | 27 || 28 | 1 | IN   | SCL.0   | 31  | 1   |
    |   5 |  21 | GPIO.21 |   IN | 1 | 29 || 30 |   |      | 0v      |     |     |
    |   6 |  22 | GPIO.22 |   IN | 1 | 31 || 32 | 0 | IN   | GPIO.26 | 26  | 12  |
    |  13 |  23 | GPIO.23 |   IN | 0 | 33 || 34 |   |      | 0v      |     |     |
    |  19 |  24 | GPIO.24 | ALT0 | 0 | 35 || 36 | 0 | IN   | GPIO.27 | 27  | 16  |
    |  26 |  25 | GPIO.25 |   IN | 0 | 37 || 38 | 0 | ALT0 | GPIO.28 | 28  | 20  |
    |     |     |      0v |      |   | 39 || 40 | 0 | ALT0 | GPIO.29 | 29  | 21  |
    +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
    | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
    +-----+-----+---------+------+---+---Pi 3---+---+------+---------+-----+-----+

    $ gpio export 17 OUTPUT
    $ gpio export 27 OUTPUT
    $ gpio export 23 OUTPUT
    $ gpio export 24 OUTPUT
    $ gpio export 22 INPUT

## Training
