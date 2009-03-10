#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "tiff2bitmap.h"
#include "jpegenc.h"
#include "logger.h"

#define ERR(...)  msg.error(__VA_ARGS__)
#define INF(...)  msg.info(__VA_ARGS__)
#define DBG(...)  msg.debug(__VA_ARGS__)

class Config
{
public:
  long  width;
  long  height;
  long  quality;
  const char* source_filename;
  const char* output_filename;
  Logger::LEVEL msg_level;

  Config() {
    width = 0;
    height = 0;
    quality = 100;
    source_filename = NULL;
    output_filename = NULL;
    msg_level = Logger::ERROR;
  };
};  


static void show_version(Logger& msg)
{
  ERR("tifftojpeg version 0.1\n");
  ERR("   Copyright 2009 Toshiyuki Suzumura, under GPL license.\n\n");
}

static void show_usage(const char* progname, Logger& msg)
{
  ERR(" Usage: %s [options] <TIFF-file> [write file|--]\n", progname);
  ERR("   -h           :show this message.\n");
  ERR("   -v           :verbose outputs.\n");
  ERR("   -vv          :very verbose outputs.\n");
  ERR("   -q <quality> :JPEG quality(1-100). (default 100)\n");
  ERR("   --           :write to stdout.\n");
}


static void parse_args(int argc, const char* argv[], Config& cfg, Logger& msg)
{
  DBG("parse_args\n");

  if(argc==1) {
    show_version(msg);    
    throw "";
  }
  for(int it=1; it<argc; it++) {
    const char* arg = argv[it];
    int sr = 0;
    DBG("- %s\n", arg);
    if(strcmp(arg, "-h")==0) {
      show_version(msg);
      show_usage(argv[0], msg);
      throw "";
   } else
    if(strcmp(arg, "-v")==0) {
      cfg.msg_level = Logger::INFO;
    } else
    if(strcmp(arg, "-vv")==0) {
      cfg.msg_level = Logger::DEBUG;
     } else
    if(strcmp(arg, "-q")==0) {
       if(it+1<argc) {
        sr = sscanf(argv[it+1], "%ld", &cfg.quality);
      }
      if(sr!=1 || cfg.quality<1 || cfg.quality>100) {
        ERR("Illegal option for '-q'.\n");
        throw "-q'";
      }
      it++;
    } else
    if(strcmp(arg, "--") && arg[0]=='-') {
      ERR("Illegal option: %s\n", arg);
      throw "-?";
    } else
    if(!cfg.source_filename) {
      cfg.source_filename = arg;
    } else {
      cfg.output_filename = arg;
    }
  }
}


int main(int argc, const char*argv[])
{
  Logger  msg;
  Tiff2bitmap   tiff;
  JpegEncoder   jpegenc;
  Config  cfg;

  msg.set_level(Logger::ERROR);
  tiff.set_logger(msg);
  jpegenc.set_logger(msg);

  // Check arguments.
  try {
    parse_args(argc, argv, cfg, msg);
  }
  catch(const char* e) {
    return -1;
  }
  msg.set_level(cfg.msg_level);
  DBG("cfg.quality: %d\n", cfg.quality);
  DBG("cfg.source_filename: %s\n", cfg.source_filename);
  DBG("cfg.output_filename: %s\n", cfg.output_filename);
  if(cfg.source_filename==NULL) {
    ERR("TIFF-file is not specified.\n");
    return -1;
  }
  if(cfg.output_filename==NULL) {
    ERR("Output-file is not specified.\n");
    return -1;
  }
 
  // Open TIFF.
  try {
    tiff.open(cfg.source_filename);
  }
  catch(Tiff2bitmapException *e) {
    ERR("Tiff2bitmapException: %s\n", e->get_text());
    return -2;
  }

  // Render all images
  try {
    int count = 1;
    char  filenamebuf[1024];
    do {
      // Read image
      uint32  width = tiff.get_width();
      uint32  height = tiff.get_height();
      uint32* img = tiff.read_RGBA_image();

      // Write to file by JPEG.
      snprintf(filenamebuf, sizeof(filenamebuf), cfg.output_filename, count);
      jpegenc.open(filenamebuf);
      jpegenc.set_size(width, height);
      jpegenc.set_quality(cfg.quality);
      jpegenc.encode((const unsigned char*)img);
      jpegenc.close();

      tiff.free_image();
      count++;
    } while(tiff.read_directory()>0);
  }

  catch(Tiff2bitmapException *e) {
    ERR("Tiff2bitmapException: %s\n", e->get_text());
  }
  catch(JpegEncoderException *e) {
    ERR("JpegEncoderException: %s\n", e->get_text());
  }

  return 0;
}
