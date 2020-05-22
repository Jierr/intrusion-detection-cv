PHONY: build

build:
	g++ -O3 ArgumentParser.cpp EMailNotifier.cpp Persistence.cpp main.cpp -o tapocam -lopencv_video -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_imgcodecs -lopencv_videoio

install:
	./install.sh

uninstall:
	./uninstall.sh
