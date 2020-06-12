CPP:=g++
CPPFLAGS:= -Wall
CPPFLAGS_RELEASE:= -O3
LFLAGS:=-lopencv_video -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_imgcodecs -lopencv_videoio -pthread

scriptsrc:=idcv-sendEmail.sh
scriptdst:=$(patsubst %.sh,bin/%.sh,$(scriptsrc))

cppsrc:= $(wildcard *.cpp)
obj:= $(patsubst %.cpp,obj/%.o,$(cppsrc))

project:=bin/tapocam

.PHONY: env clean all debug release scripts install uninstall


extend_cflag: 
	$(eval CPPFLAGS += $(CPPFLAGS_RELEASE))

all: $(project)

release: | extend_cflag $(project) 

debug: $(project)

env:
	mkdir -p bin
	mkdir -p obj
	
clean:
	rm -rf bin/*
	rm -rf obj/*
	
$(project): env scripts $(obj) 
	$(CPP) -o $(project) $(obj) $(LFLAGS)	

obj/%.o: %.cpp
	$(CPP) -c -o $@ $< $(CPPFLAGS)
	
scripts: env $(scriptdst)

bin/%.sh: %.sh
	cp $< $@

install:
	./install.sh

uninstall:
	./uninstall.sh
