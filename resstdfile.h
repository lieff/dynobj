#pragma once
#include "resfile.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

// Standart C file implementation

class CStandartResFile: public CResFile
{
    FILE *f;
public:
    // constructors
    CStandartResFile()
    {
        f = 0;
    }
    CStandartResFile(FILE *f)
    {
        this->f = f;
    }
    CStandartResFile(const char *filename)
    {
        f = ::fopen(filename, "rb");
    }
    CStandartResFile(const char *filename, const char *mode)
    {
        f = ::fopen(filename, mode);
    }
    ~CStandartResFile()
    {
        if (f) fclose(f);
    }
    // CResFile
    size_t Read(void *p, size_t size)
    {
        return fread(p, 1, size, f);
    }
    size_t Write(const void *p, size_t size)
    {
        return fwrite(p, 1, size, f);
    }
    size_t GetSize()
    {
        if (!f) return -1;
#ifdef _WIN32
        size_t pos = ftell(f);
        if (fseek(f, 0, SEEK_END))
            return 0;
        size_t size = ftell(f);
        if (fseek(f, pos, SEEK_SET))
            return 0;
        return size;
#else  //_WIN32
        struct stat s;
        if (fstat(fileno(f), &s) != 0) return 0;
        //if (s.st_size > 0xffffffff) return -1;
        return (size_t)s.st_size;
#endif //_WIN32
    }
    size_t GetPos()
    {
        if (!f) return -1;
        return ftell(f);
    }
    int SetPos(size_t pos)
    {
        if (!f) return 0;
        return !fseek(f, (long)pos, SEEK_SET);
    }
    int IsOpened()
    {
        return f != 0;
    }
    // CStandartResFile
    int Open(const char *filename)
    {
        Close();
        f = ::fopen(filename, "wb");
        return (f != 0);
    }
    int Close()
    {
        if (f) fclose(f); f = 0;
        return 1;
    }
    int SetFile(FILE *f)
    {
        Close();
        this->f = f;
    }
    static CResFile *fopen(const char *filename, const char *mode)
    {
        CResFile *f = new CStandartResFile(filename, mode);
        if (!f) return 0;
        if (!f->IsOpened()) { delete f; return 0; }
        return f;
    }
    void rewind()
    {
        ::rewind(f);
    }
    operator FILE*()
    {
        return f;
    }
};
