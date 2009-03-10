EXE=tifftojpeg
OBJS=tiff2jpeg.o tiff2bitmap.o jpegenc.o logger.o
PREFIX=/usr/local

#DEBUG_CFLAGS= -g
#GPROF_FLAG = -pg

CFLAGS= -O2 -Wall -W `Magick++-config --cppflags --cxxflags`$(DEBUG_CFLAGS)
LDFLAGS= `Magick++-config --ldflags --libs` -lstdc++ -ljpeg -ltiff

all:$(EXE)

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $(EXE) $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm $(EXE) *.o *.bak *.out -f

install: $(SHAREDLIBV)
	-@if [ ! -d $(PREFIX)/bin ]; then mkdir -p $(PREFIX)/bin; fi
	install $(EXE) $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/bin/$(EXE)
