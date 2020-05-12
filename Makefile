PHONY: all

all:
	g++ main.cpp -o tapocam -lopencv_video -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_imgcodecs -lopencv_videoio
