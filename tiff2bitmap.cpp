#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <error.h>

#include "tiff2bitmap.h"

#define ERR(...)  m_msg->error("t2bmp: "__VA_ARGS__)
#define INF(...)  m_msg->info("t2bmp: "__VA_ARGS__)
#define DBG(...)  m_msg->debug("t2bmp: "__VA_ARGS__)


const char* Tiff2bitmapException::get_text()
{
    if(m_txt) return m_txt;
    return strerror(m_errno);
}


Tiff2bitmap::Tiff2bitmap()
{
  m_tiff = NULL;
  m_fd = -1;
  m_img = NULL;
  m_imglen = 0;
  m_template[0] = '\0';
  m_msg = new Logger;
}


Tiff2bitmap::~Tiff2bitmap()
{
  free_image();
  close();
}


void Tiff2bitmap::open(const char* file_name)
{
  close();
  m_tiff = TIFFOpen(file_name, "r");
  if(!m_tiff) throw Tiff2bitmapException(errno);
}

  
void Tiff2bitmap::close()
{
  if(m_tiff)  TIFFClose(m_tiff);
  m_tiff = NULL;
}


void  Tiff2bitmap::make_tempname()
{
  char*  tmpenv=getenv("TMP");
  if(!tmpenv) tmpenv=getenv("TMPDIR");
  if(!tmpenv) tmpenv=getenv("TMP_DIR");
  if(!tmpenv) tmpenv="/tmp";
  if(tmpenv[0]=='/') tmpenv++;
  snprintf(m_template, sizeof(m_template), "/%s/tiff2bitmap-XXXXXX", tmpenv);
}


uint32* Tiff2bitmap::read_RGBA_image()
{
  uint32 width = get_width();
  uint32 height = get_height();

  make_tempname();
  m_imglen = sizeof(uint32)*width*height;
  try {
    m_fd = mkstemp(m_template);
    if(m_fd<0) {
      ERR("read_RGBA_image::mkstemp() failed.\n");
      throw errno;
    }
    if(lseek(m_fd, m_imglen-1, SEEK_SET)<0) {
      ERR("read_RGBA_image::lseek()-1 failed.\n");
      throw errno;
    }
    if(write(m_fd, "", 1)<0) {
      ERR("read_RGBA_image::write(1) failed.\n");
      throw errno;
    }
    if(lseek(m_fd, 0, SEEK_SET)<0) {
      ERR("read_RGBA_image::lseek()-2 failed.\n");
      throw errno;
    }
    m_img = (uint32*)mmap(NULL, m_imglen, PROT_READ|PROT_WRITE, MAP_PRIVATE, m_fd, 0);
    if(!m_img) {
      ERR("read_RGBA_image::mmap() failed.\n");
      throw errno;
    }
  }
  catch(int e) {
    free_image();
    throw Tiff2bitmapException(e);
  }

  DBG("read_RGBA_image(%dx%d)=>%p[%d].\n", width, height, m_img, m_imglen);
  if(TIFFReadRGBAImageOriented(m_tiff, width, height, m_img, ORIENTATION_TOPLEFT)==0) {
    ERR("TIFFReadRGBAImageOriented(%dx%d) failed.\n", width, height);
    free_image();
    throw Tiff2bitmapException("TIFFReadRGBAImageOrirnted failed.");
  }

  return m_img;
}


void Tiff2bitmap::free_image()
{
  if(m_img) munmap(m_img, m_imglen);
  if(m_fd>=0) ::close(m_fd);
  if(m_template[0]) unlink(m_template);
  m_img = NULL;
  m_imglen = 0;
  m_fd = -1;
  m_template[0]='\0';
}


uint32  Tiff2bitmap::get_field32(ttag_t tag, uint32 def)
{
  uint32  v;
  if(TIFFGetField(m_tiff, tag, &v)==0) {
    ERR("TIFFGetField32(%d) failed.\n", tag);
    v = def;
    // throw Tiff2bitmapException(errno);
  }
  return v;
}


uint16  Tiff2bitmap::get_field16(ttag_t tag, uint16 def)
{
  uint16  v;
  if(TIFFGetField(m_tiff, tag, &v)==0) {
    ERR("TIFFGetField16(%d) failed.\n", tag);
    v = def;
    // throw Tiff2bitmapException(errno);
  }
  return v;
}
