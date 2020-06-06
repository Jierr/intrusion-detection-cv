CPP:=g++
CPPFLAGS:=-O3 -Wall
LFLAGS:=-lopencv_video -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_imgcodecs -lopencv_videoio -pthread

scriptsrc:=send.sh
scriptdst:=$(patsubst %.sh,bin/%.sh,$(scriptsrc))

cppsrc:= $(wildcard *.cpp)
obj:= $(patsubst %.cpp,obj/%.o,$(cppsrc))

project:=bin/tapocam

.PHONY: all scripts env clean install uninstall

all: $(project)

env:
	mkdir -p bin
	mkdir -p obj
	
clean:
	rm -rf bin
	rm -rf obj
	
$(project): env scripts $(obj) 
	$(CPP) -o $(project) $(obj) $(LFLAGS)	

scripts: env $(scriptdst)

bin/%.sh: %.sh
	cp $< $@

obj/%.o: %.cpp
	$(CPP) -c -o $@ $< $(CPPFLAGS)

install:
	./install.sh

uninstall:
	./uninstall.sh
