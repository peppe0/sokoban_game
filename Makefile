CXX = /usr/bin/clang++
CXXFLAGS = -std=c++17 -Wall -g -Wno-deprecated
INCLUDES = -I./dependencies/include -I/opt/homebrew/include/freetype2 -I/opt/homebrew/include
LIBDIRS = -L./dependencies/library -L/opt/homebrew/lib
LIBS = ./dependencies/library/libglfw.3.4.dylib ./dependencies/library/libGLEW.2.2.0.dylib \
       -framework OpenGL -framework Cocoa -framework IOKit \
       -framework CoreVideo -framework CoreFoundation -framework GLUT \
       -lfreetype -lassimp
SRCS = $(wildcard ./*.cpp)
TARGET = sokoban

all: $(TARGET)

$(TARGET): $(SRCS) glad.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBDIRS) $(SRCS) glad.c -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
