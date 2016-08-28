#!/bin/bash

gpio export 17 OUTPUT
gpio export 27 OUTPUT
gpio export 23 OUTPUT
gpio export 24 OUTPUT

./build/bin/car
