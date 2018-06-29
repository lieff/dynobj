#pragma once

#define SYMBOL_TYPE_CODE               0x01
#define SYMBOL_TYPE_INITIALIZED_DATA   0x02
#define SYMBOL_TYPE_UNINITIALIZED_DATA 0x04
#define SYMBOL_TYPE_WRITE              0x08

#define FUNCTION_TYPE_CREATE_POINTER 0x01


enum FunctionType
{ 
    ft_cdecl,    // Pushes parameters on the stack, in reverse order (right to left)
    ft_stdcall,  // Pushes parameters on the stack, in reverse order (right to left)
    ft_fastcall  // Stored in registers, then pushed on stack
    //ft_thiscall  // Pushed on stack; this pointer stored in ECX
};

struct cfunction
{
    char *name;
    void *address;
    int type;
};

class CResFile;

class CCodeLib
{
    struct reloc
    {
        void **VirtualAddress;
        int imp_num;
    };
    struct symbol
    {
        char *name;
        char *undec_name;
        void *address;
        int size, type;
    };
    int alloctype, image_ready, image_size, bbs_size, code_size, data_size, rdata_size;
    BYTE *pimage;
    BYTE *bss_seg;

public:
    CCodeLib();

    int exports_num;
    symbol *exports;
    int imports_num, resolved_imports_num;
    symbol *imports;
    int relocs_num;
    reloc  *relocs;

    int LoadObjCode(CResFile *f);
    //int LoadDll(char *dllname,char **funcs);
    int LoadProcs(cfunction *funcs);
    int Link(CCodeLib *lib);
    int LinkMany(CCodeLib **lib, int libs);
    void Unload();

    void *FindSymAddr(char *fname);
    void *FindSymAddrUndec(char *und_fname);

    symbol *FindSym(char *sym_name);
    symbol *FindSymUndec(char *und_sym_name);

    int operator ()(char *fn_name);

    int GetBSS_Size()  { return bbs_size;  }
    int GetCode_Size() { return code_size; }
    int GetData_Size() { return data_size; }
    int GetRData_Size() { return rdata_size; }
};
