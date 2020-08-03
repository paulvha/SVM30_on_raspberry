# Sensirion SVM30 on Raspberry Pi

## ===========================================================

A program to set instructions and get information from an SVM30. It has been tested to run on Raspberry Pi
<br> A detailed description of the options and findings are available in the svm30.odt

<br>A version for Arduino & variants is available on ![https://github.com/paulvha/svm30](https://github.com/paulvha/svm30)

## Getting Started
As part of a larger project I am looking at analyzing and understanding the air quality.
I have done a number of projects on air-sensors. The SVM30 sensor is a new kid on the block
that looks interesting. This is a working driver + user level program.

A word of warning: the SVM30 needs a female plug, which is different depending on the rpoduct version of your board

SVM30-Y  Yeonho Electronics, 20037WR-04 <br>
SVM30-J  Scondar SCT2001WR -S-4P compatible to JST part no. S4B-PH-SM4-TB <br>

## Prerequisites
Install latest from BCM2835 from : http://www.airspayce.com/mikem/bcm2835/

## Software installation
1. wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.56.tar.gz
2. tar -zxf bcm2835-1.56.tar.gz     // 1.56 was version number at the time of writing
3. cd bcm2835-1.56
4. ./configure
5. sudo make check
6. sudo make install

To create a build with only the SVM30 monitor type:
     make
To create a build with the SVM30 and SDS011 monitor type:
     make BUILD=SDS011 (requires the sds011 sub-directory)

## Program usage
### SVM30 settings:
    -c 0x#  set baseline CO2  to ####
    -t 0x#  set baseline TVOC to ####
    -h      continued humidity compensation
    -m      perform a measurement test
###  program control settings:
    -d       display ID-numbers and feature set only
    -l #     number of measurements (0 = endless)
    -w #     wait-time (seconds) between measurements
    -v       include verbose / debug information

### output formatting
    -D      do not display output in color
    -T      add / remove timestamp
    -H      add / remove humidity & temperature
    -A      add / remove CO2 / TVOC info"
    -B      add / remove baseline info
    -R      add / remove H2 and Ethanol signals
    -E      add / remove Dew point calculation
    -J      add / remove Absolute Humidity calc
    -G      add / remove HeatIndex calc
    -F      Display temperature (Fahrenheit/Celsius)

## Versioning

### Version 1.1 / October 2019
 * added dewPoint and heatindex
 * added temperature selection (Fahrenheit / Celsius)

## Author
 * Paul van Haastrecht (paulvha@hotmail.com)

## License
This project is licensed under the GNU GENERAL PUBLIC LICENSE 3.0

## Acknowledgements
Make sure to read the SVM30, SHTC1 and SGP30 datasheets from Sensirion.
They provide good starting point for information and are added in the extras directory.<br>
Philipp : Extra clarification on about female plug
