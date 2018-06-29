#pragma once
#include <stddef.h>

class CResFile
{
public:
  virtual ~CResFile() {};
  virtual size_t Read(void *p, size_t size)=0; // return 0 on error
  virtual size_t Write(const void *p, size_t size)=0; // return 0 on error
  virtual size_t GetSize()=0;                 // return -1 on error
  virtual size_t GetPos()=0;                  // return -1 on error
  virtual int SetPos(size_t pos)=0;           // return 0 on error
  virtual int IsOpened()=0;                   // return 0 if object not connected with storage
};
