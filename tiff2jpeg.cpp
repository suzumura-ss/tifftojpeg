#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <Magick++.h>

#include "tiff2bitmap.h"
#include "jpegenc.h"
#include "logger.h"

#define ERR(...)  msg.error(__VA_ARGS__)
#define INF(...)  msg.info(__VA_ARGS__)
#define DBG(...)  msg.debug(__VA_ARGS__)

class Config
{
public:
  long  quality;
  const char* source_filename;
  const char* output_filename;
  const char* resize;
  Logger::LEVEL msg_level;

  Config() {
    quality = 100;
    source_filename = NULL;
    output_filename = NULL;
    resize = NULL;
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
  ERR("   -h             :show this message.\n");
  ERR("   -v             :verbose outputs.\n");
  ERR("   -vv            :very verbose outputs.\n");
  ERR("   -q <quality>   :JPEG quality(1-100). (default 100)\n");
  ERR("   -r <geometory> :resize geometory\n");
  ERR("   --             :write to stdout.\n");
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
    if(strcmp(arg, "-r")==0) {
      if(it+1<argc) {
        cfg.resize = argv[it+1];
      } else {
        ERR("Illegal option for '-r'.\n");
        throw "-r'";
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


void  write_to_jpeg_direct(Tiff2bitmap& tiff, int count, Logger& msg, Config& cfg)
{
  uint32  width = tiff.get_width();
  uint32  height = tiff.get_height();
  uint32* img = tiff.read_RGBA_image();
  char  strbuf[1024];

  JpegEncoder jpegenc;
  jpegenc.set_logger(msg);

  DBG("- writing directry.\n");
  snprintf(strbuf, sizeof(strbuf), cfg.output_filename, count);
  jpegenc.open(strbuf);
  jpegenc.set_size(width, height);
  jpegenc.set_quality(cfg.quality);
  jpegenc.encode((const unsigned char*)img);
  jpegenc.close();

  tiff.free_image();
}


void  resize_directry(Tiff2bitmap& tiff, int count, Logger& msg, Config& cfg)
{
  uint32  width = tiff.get_width();
  uint32  height = tiff.get_height();
  uint32* img = tiff.read_RGBA_image();
  char  strbuf[1024];

  DBG("- creating Image object.\n");
  Magick::Image mgk(width, height, "RGBA", Magick::CharPixel, img);

  DBG("- resizing.\n");
  mgk.resize(cfg.resize);

  DBG("- writing.\n");
  mgk.quality(cfg.quality);
  snprintf(strbuf, sizeof(strbuf), cfg.output_filename, count);
  mgk.write(strbuf);

  tiff.free_image();
}

 
void  resize_tilely(Tiff2bitmap& tiff, int count, Logger& msg, Config& cfg)
{
  uint32  width = tiff.get_width();
  uint32  height = tiff.get_height();
  uint32  resize_width=0, resize_height=0;
  char  strbuf[1024], resizeopt[2]="";
  uint32  off_w=0, off_h=0, w_idx=0, h_idx=0;

  // Calcurate tile division
  sscanf(cfg.resize, "%dx%d", &resize_width, &resize_height);
  resizeopt[0] = cfg.resize[strlen(cfg.resize)-1];
  if(resizeopt[0]!='>' && resizeopt[0]!='<') {
    resizeopt[0]='\0';
  }

  if(width<=resize_width || height<=resize_height) {
    // Don't enlarge
    DBG("resize (%dx%d) to (%dx%d) - do directry.\n", width, height, resize_width, resize_height);
    resize_directry(tiff, count, msg, cfg);
    return;
  }

  float rrw = width /resize_width;
  float rrh = height/resize_height;
  float resize_rate = (rrw>rrh)? rrw: rrh;
  DBG("- resize_rate = %f (%fx%f)\n", resize_rate, rrw, rrh);

  // Tileing
  uint32* img = tiff.read_RGBA_image();
  uint32  im_width  = (width /1024)+1; // image matrix width
  uint32  im_height = (height/1024)+1; // image matrix height
  Magick::Image rimg(
              Magick::Geometry((uint32)(width*1.1/resize_rate), (uint32)(height*1.1/resize_rate), 0, 0),
              Magick::Color(0, 0, 0, 0));

  uint32  rimg_ofs_h = 0;
  for(h_idx=0; h_idx<im_height; h_idx++) {
    uint32  rimg_ofs_w = 0;
    for(w_idx=0; w_idx<im_width; w_idx++) {
      off_w = w_idx*1024;
      off_h = h_idx*1024;
      uint32  mw = (width -off_w)>1024 ? 1024: width -off_w;
      uint32  mh = (height-off_h)>1024 ? 1024: height-off_h;
      INF("- Tile processing (%d,%d)(%d, %d)-[%dx%d]-[%dx%d]\n",
            w_idx, h_idx, rimg_ofs_w, rimg_ofs_h,
            off_w, off_h, mw, mh);

      // Duplicate to tile and create Image object
      DBG(" - duplicate\n");
      uint32* tile = (uint32*)malloc(mw*mh*sizeof(uint32));
      uint32* base = img + off_h*width + off_w;
      // DBG("  %p <= %p, %d\n", tile, base, mw*mh*sizeof(uint32));
      for(uint32 ih=0; ih<mh; ih++) {
        // DBG("  [%4d] %p-%p:%p <= %p:%p\n", ih, tile, tile+mh*mw, tile+ih*mw, base, base+ih*width);
        memcpy(tile+ih*mw, base+ih*width, sizeof(uint32)*mw);
      }
      DBG(" - create-image\n");
      Magick::Image ti(mw, mh, "RGBA", Magick::CharPixel, tile);

      // Resize and store
      uint32 rw = (uint32)(((float)mw)*1.1/resize_rate);
      uint32 rh = (uint32)(((float)mh)*1.1/resize_rate);
      DBG(" - resize: %dx%d\n", rw, rh);
      ti.resize(Magick::Geometry(rw, rh, 0, 0));

      // Montagea
      DBG(" - montage: (%d,%d)\n", rimg_ofs_w, rimg_ofs_h);
      rimg.composite(ti, rimg_ofs_w, rimg_ofs_h);
      rimg_ofs_w += ti.columns();
      if(w_idx+1==im_width) rimg_ofs_h += ti.rows();
      free(tile);
    }
  }

  // Final resize
  DBG("- Final resize.\n");
  rimg.trim();
  rimg.resize(cfg.resize);

  // Write to file
  DBG("- Write to file.\n");
  rimg.quality(cfg.quality);
  snprintf(strbuf, sizeof(strbuf), cfg.output_filename, count);
  rimg.write(strbuf);

  // Finalize
  tiff.free_image();
}


int main(int argc, const char*argv[])
{
  Logger  msg;
  Tiff2bitmap   tiff;
  Config  cfg;
  

  msg.set_level(Logger::ERROR);
  tiff.set_logger(msg);

  // Check arguments.
  try {
    parse_args(argc, argv, cfg, msg);
  }
  catch(const char* e) {
    return -1;
  }
  msg.set_level(cfg.msg_level);
  DBG("cfg.quality: %ld\n", cfg.quality);
  DBG("cfg.resize: %s\n", cfg.resize);
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
    do {
      INF("Processing %d..\n", count);
      // Read image
      uint32  width = tiff.get_width();
      uint32  height = tiff.get_height();

      if(cfg.resize) {
        // Resize
        if(width>2048 || height>2048) {
          resize_tilely(tiff, count, msg, cfg);
        } else {
          resize_directry(tiff, count, msg, cfg);
        }
     } else {
        write_to_jpeg_direct(tiff, count, msg, cfg);
     }

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
