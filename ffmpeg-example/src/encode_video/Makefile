CC = gcc
CXX = g++
CFLAGS = -Wall -g
CXXFLAGS = -Wall -g
LDFLAGS = -lavcodec -lavutil

OUTPUT_DIR = ../../../ffmpeg-example/release
TARGET = $(OUTPUT_DIR)/encode_video

SRCS = encode_video.cpp

OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS)
