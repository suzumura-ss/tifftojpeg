#ifndef __INCLUDE_TIFF2BITMAP_H__
#define __INCLUDE_TIFF2BITMAP_H__

#include <tiffio.h>
#include "logger.h"

class Tiff2bitmapException
{
public:
  Tiff2bitmapException() {
    m_errno = 0;
    m_txt = NULL;
  };
  Tiff2bitmapException(int error) {
    m_errno = error;
    m_txt = NULL;
  };
  Tiff2bitmapException(const char* txt) {
    m_errno = 0;
    m_txt = txt;
  };

  virtual ~Tiff2bitmapException() {};
  const char* get_text();

private:
  int m_errno;
  const char* m_txt;
};


class Tiff2bitmap
{
public:
  Tiff2bitmap();
  ~Tiff2bitmap();

  void  open(const char* file_name);
  void  close();

  uint32  get_width() {
    return get_field32(TIFFTAG_IMAGEWIDTH);
  };
  uint32  get_height() {
    return get_field32(TIFFTAG_IMAGELENGTH);
  };
  int read_directory() {
    return TIFFReadDirectory(m_tiff);
  };

  uint32* read_RGBA_image();
  void free_image();
  uint32  get_field32(ttag_t tag, uint32 def=(uint32)-1);
  uint16  get_field16(ttag_t tag, uint16 def=(uint16)-1);

  void  set_logger(Logger& logger) {
    delete m_msg;
    m_msg = &logger;
  };

protected:
  void  make_tempname();

private:
  Logger* m_msg;
  TIFF*   m_tiff;
  int     m_fd;
  uint32* m_img;
  off_t   m_imglen;
  char    m_template[256];
};

#endif //__INCLUDE_TIFF2BITMAP_H__
