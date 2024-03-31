CC = g++
CFLAGS = -Wall -O2
LIBS = -lavcodec -lavformat -lavutil

SRCS = transcode.cpp video_debugging.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = video_debugging.h

OUTPUT_DIR = ../../../ffmpeg-example/release
TARGET = $(OUTPUT_DIR)/transcode

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
