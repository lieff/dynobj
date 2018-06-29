#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <imagehlp.h>

#include "codelib.h"
#include "resfile.h"

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = 0; } }
#endif

CCodeLib::CCodeLib()
{
    alloctype   = 0;
    image_ready = 0;
    image_size  = bbs_size = 0;
    pimage      = 0;
    bss_seg     = 0;
    exports_num = 0;
    exports     = 0;
    imports_num = 0;
    resolved_imports_num = 0;
    imports     = 0;
    relocs_num  = 0;
    relocs      = 0;
}

void CCodeLib::Unload()
{
    if (pimage) 
        if (alloctype) VirtualFree(pimage, 0, MEM_RELEASE); else delete pimage;
    SAFE_DELETE(bss_seg);
    image_size = bbs_size = 0;
    pimage      = 0;
    alloctype   = 0;
    image_ready = 0;

    int i;
    for(i = 0; i < exports_num; i++)
        if (exports[i].name) delete exports[i].name;
    for(i = 0; i < imports_num; i++)
        if (imports[i].name) delete imports[i].name;
    exports_num = 0;
    imports_num = 0;
    resolved_imports_num = 0;
    relocs_num  = 0;
    SAFE_DELETE(exports);
    SAFE_DELETE(imports);
    SAFE_DELETE(relocs);
}

int CCodeLib::LoadObjCode(CResFile *f)
{
    Unload();

    int imagesize = f->GetSize();
    pimage = (BYTE*)VirtualAlloc(0, imagesize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!pimage)
    {
        printf("error: not enough memory %d", imagesize);
        return 0;
    }
    alloctype = 1;
    f->Read(pimage, imagesize);

    // setup pointers
    IMAGE_FILE_HEADER *fh = (IMAGE_FILE_HEADER *)pimage;
    if (fh->Machine != IMAGE_FILE_MACHINE_I386 || fh->SizeOfOptionalHeader != 0)
    {
        printf("error: invalid header");
        return 0;
    }

    IMAGE_SECTION_HEADER *sections = (IMAGE_SECTION_HEADER *)(pimage + sizeof(IMAGE_FILE_HEADER));
    BYTE **sections_data = new BYTE *[fh->NumberOfSections];
    int i;
    // calculate section sizes statistics
    bbs_size  = 0;
    code_size = 0;
    data_size = 0;
    rdata_size = 0;
    for (i = 0; i < fh->NumberOfSections; i++)
    {
        if (sections[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
        {
            sections_data[i] = 0;
            bbs_size += sections[i].SizeOfRawData;
        } else
            sections_data[i] = pimage + sections[i].PointerToRawData;
        if (sections[i].Characteristics & IMAGE_SCN_CNT_CODE)
        {
            code_size += sections[i].SizeOfRawData;
        } else
        if (sections[i].Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA &&
            !(sections[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
            )
        {
            if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE)
                data_size  += sections[i].SizeOfRawData;
            else
                rdata_size += sections[i].SizeOfRawData;
        } 
    }
    // allocate bss section if needed
    if (bbs_size)
    {
        bss_seg = new BYTE[bbs_size];
        ZeroMemory(bss_seg, bbs_size);
        BYTE *pbbs = bss_seg;
        for (i = 0; i < fh->NumberOfSections; i++)
            if (sections[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
            {
                sections_data[i] = pbbs;
                pbbs += sections[i].SizeOfRawData;
            }
    }
    IMAGE_SYMBOL *symbols = (IMAGE_SYMBOL *)(pimage + fh->PointerToSymbolTable);

    // process symbols
    exports_num = 0;
    imports_num = 0;
    for (i = 0; i < (int)fh->NumberOfSymbols; i++)
    {
        if (symbols[i].StorageClass == IMAGE_SYM_CLASS_EXTERNAL ||
           (symbols[i].StorageClass == IMAGE_SYM_CLASS_STATIC &&
            symbols[i].SectionNumber > 0 && symbols[i].NumberOfAuxSymbols == 0))     // TODO: remove static members
            if (symbols[i].SectionNumber == IMAGE_SYM_UNDEFINED)
                imports_num++; else exports_num++;
        i += symbols[i].NumberOfAuxSymbols;
    }
    exports = new symbol[exports_num];
    if (!exports) return 0;
    imports = new symbol[imports_num];
    if (!imports) return 0;

    char *string_table = (char*)&symbols[fh->NumberOfSymbols];
    int nexp = 0, nimp = 0;
    for (i = 0; i < (int)fh->NumberOfSymbols; i++)
    {
        if (symbols[i].StorageClass == IMAGE_SYM_CLASS_EXTERNAL ||
           (symbols[i].StorageClass == IMAGE_SYM_CLASS_STATIC && 
            symbols[i].SectionNumber > 0 && symbols[i].NumberOfAuxSymbols == 0))     // TODO: remove static members
        {
            int len = 0;
            char *fname;
            char buf[256];
            if (symbols[i].N.Name.Short != 0)
            {
                fname = (char*)&symbols[i].N.ShortName;
                sprintf(buf, "%.8s", fname);
                fname = buf;
                len   = strlen(fname);
            } else
            {
                fname = string_table + symbols[i].N.Name.Long;
                len   = strlen(fname);
            }
            int undlen = UnDecorateSymbolName(fname, buf, 256, UNDNAME_NAME_ONLY);
            char *undec_name = new char[undlen + 1];
            strcpy(undec_name, buf);

            if (symbols[i].SectionNumber == IMAGE_SYM_UNDEFINED)
            {
                imports[nimp].undec_name = undec_name;
                imports[nimp].name = new char[len + 1];
                strcpy(imports[nimp].name, fname);

                imports[nimp].address = 0;
                imports[nimp].size    = 0;
                imports[nimp].type    = 0;
                symbols[i].Value      = nimp;
                nimp++;
            } else
            {
                exports[nexp].undec_name = undec_name;
                exports[nexp].name = new char[len + 1];
                strcpy(exports[nexp].name, fname);

                int sect = symbols[i].SectionNumber - 1;
                exports[nexp].address = sections_data[sect] + symbols[i].Value;
                exports[nexp].size = sections[sect].SizeOfRawData - symbols[i].Value;
                // correct size if any other symbols present in this section
                for (int j = 0; j < (int)fh->NumberOfSymbols; j++)
                    if (i != j && (symbols[j].SectionNumber - 1) == sect)
                    {
                        if (symbols[j].Value > symbols[i].Value)
                        {
                            int size = symbols[j].Value - symbols[i].Value;
                            if (exports[nexp].size > size)
                                exports[nexp].size = size;
                            break;
                        }
                    }
                exports[nexp].type = 0;
                if (sections[sect].Characteristics & IMAGE_SCN_CNT_CODE) 
                    exports[nexp].type |= SYMBOL_TYPE_CODE;
                if (sections[sect].Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) 
                    exports[nexp].type |= SYMBOL_TYPE_INITIALIZED_DATA;
                if (sections[sect].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) 
                    exports[nexp].type |= SYMBOL_TYPE_UNINITIALIZED_DATA;
                if (sections[sect].Characteristics & IMAGE_SCN_MEM_WRITE) 
                    exports[nexp].type |= SYMBOL_TYPE_WRITE;
                nexp++;
            }
        }
        if (symbols[i].NumberOfAuxSymbols)
        {
            IMAGE_AUX_SYMBOL *aux = (IMAGE_AUX_SYMBOL *)&symbols[i + 1];
            aux = aux;
        }

        i += symbols[i].NumberOfAuxSymbols;
    }

    // rebase sections
    relocs_num = 0;
    for (i = 0; i < fh->NumberOfSections; i++)
    {
        /*if (strncmp((char*)&sections[i].Name[0], ".deb", 4) != 0 &&
            strncmp((char*)&sections[i].Name[0], ".dre", 4) != 0)
            printf("%s %d\n", sections[i].Name, sections[i].SizeOfRawData);*/
        if (sections[i].PointerToRelocations == 0) continue;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE) continue;
        IMAGE_RELOCATION *rel = (IMAGE_RELOCATION *)(pimage + sections[i].PointerToRelocations);
        for (int j = 0; j < sections[i].NumberOfRelocations; j++)
            if (rel[j].Type == IMAGE_REL_I386_DIR32 || 
                rel[j].Type == IMAGE_REL_I386_REL32)
            {
                void **p = (void**)(sections_data[i] + rel[j].VirtualAddress);
                int sym  = rel[j].SymbolTableIndex;
                int sect = symbols[sym].SectionNumber;
                switch (sect)
                {
                case IMAGE_SYM_UNDEFINED:
                    relocs_num++;
                    continue;
                case IMAGE_SYM_ABSOLUTE:
                case IMAGE_SYM_DEBUG:
                    continue;
                }
                void *address = sections_data[sect - 1] + symbols[sym].Value;
                if (rel[j].Type == IMAGE_REL_I386_REL32)
                    *(DWORD*)p += (DWORD)((uintptr_t)address - (uintptr_t)p - sizeof(void*));
                else
                    *(DWORD*)p += (uintptr_t)address;
            } else if (rel[j].Type != IMAGE_REL_I386_ABSOLUTE)
            {   // unsupported relocation
                printf("error: unsupported relocation");
                return 0;
            }
    }

    // build relocations for imports
    resolved_imports_num = 0;
    relocs = new reloc[relocs_num];
    int nreloc = 0;
    for (i = 0; i < fh->NumberOfSections; i++)
    {
        if (sections[i].PointerToRelocations == 0) continue;
        IMAGE_RELOCATION *rel = (IMAGE_RELOCATION *)(pimage + sections[i].PointerToRelocations);
        for (int j = 0; j < sections[i].NumberOfRelocations; j++)
            if (rel[j].Type == IMAGE_REL_I386_DIR32 || 
                rel[j].Type == IMAGE_REL_I386_REL32)
            {
                int sym  = rel[j].SymbolTableIndex;
                if (symbols[sym].SectionNumber == IMAGE_SYM_UNDEFINED)
                {
                    relocs[nreloc].VirtualAddress = (void**)(sections_data[i] + rel[j].VirtualAddress);
                    relocs[nreloc].imp_num = symbols[sym].Value;
                    if (rel[j].Type == IMAGE_REL_I386_REL32)
                        relocs[nreloc].imp_num |= 0x80000000;
                    nreloc++;
                }
            }
    }

    delete sections_data;
    return 1;
}

int CCodeLib::LoadProcs(cfunction *funcs)
{
    Unload();
    exports_num = 0;
    int npointers = 0;
    for(;;exports_num++)
    {
        if (funcs[exports_num].name == 0) break;
        if (funcs[exports_num].type & FUNCTION_TYPE_CREATE_POINTER) npointers++;
    }

    void **paddr;
    if (npointers)
    {
        pimage = (BYTE*)new DWORD[npointers];
        if (!pimage) return 0;
        alloctype = 0;
        paddr = (void **)pimage;
    }

    exports = new symbol[exports_num];
    if (!exports) return 0;

    int ppointer = 0;
    for(int i = 0; i < exports_num; i++)
    {
        int len = strlen(funcs[i].name);
        exports[i].name = new char[len + 1];
        strcpy(exports[i].name, funcs[i].name);
        char buf[256];
        int undlen = UnDecorateSymbolName(funcs[i].name, buf, 256, UNDNAME_NAME_ONLY);
        exports[i].undec_name = new char[undlen + 1];
        strcpy(exports[i].undec_name, buf);

        if (funcs[i].type & FUNCTION_TYPE_CREATE_POINTER)
        {
            paddr[ppointer]    = funcs[i].address;
            exports[i].address = &paddr[ppointer];
            ppointer++;
        } else
            exports[i].address = funcs[i].address;
        exports[i].size = 0;
        exports[i].type = SYMBOL_TYPE_CODE;
    }
    image_ready = 1;
    return 1;
}

int CCodeLib::Link(CCodeLib *lib)
{
    int i;
    if (!relocs_num || !relocs || !imports_num || !imports) return 1;
    if (lib)
    {
        if (resolved_imports_num >= imports_num)
            return 1;
        for(i = 0; i < imports_num; i++)
        {
            if (imports[i].address) continue;
            symbol *sym = lib->FindSym(imports[i].name);
            if (sym)
            {
                if (!sym->address) continue;
                imports[i].address = sym->address;
                imports[i].size    = sym->size;
                imports[i].type    = sym->type;
                resolved_imports_num++;
            }
        }
    }
/*        for(i = 0; i < imports_num; i++)
        {
            if (imports[i].address) continue;
            MessageBox(0, imports[i].name, "Error", MB_OK | MB_ICONSTOP);
        }*/
    if (resolved_imports_num >= imports_num)
    {
        if (image_ready) return 1;
        for(i = 0; i < relocs_num; i++)
        {
            int imp = relocs[i].imp_num & 0x7fffffff;
            if (imports[imp].address)
            {
                if (relocs[i].imp_num & 0x80000000) // relative ?
                *(DWORD*)relocs[i].VirtualAddress += (DWORD)((uintptr_t)imports[imp].address - (uintptr_t)relocs[i].VirtualAddress - 4);
                else
                *(DWORD*)relocs[i].VirtualAddress += (uintptr_t)imports[imp].address;
            } else return 0;
        }
        image_ready = 1;
        return 1;
    }
    return 0;
}

int CCodeLib::LinkMany(CCodeLib **lib, int libs)
{
    for(int i = 0; i < libs; i++)
    {
        if (lib[i] == 0)    continue;
        if (lib[i] == this) continue;
        Link(lib[i]);
    }
    return image_ready;
}

void *CCodeLib::FindSymAddr(char *sym_name)
{
    for(int i = 0; i < exports_num; i++)
        if (strcmp(exports[i].name, sym_name) == 0) return exports[i].address;
    return 0;
}

void *CCodeLib::FindSymAddrUndec(char *und_fname)
{
    for(int i = 0; i < exports_num; i++)
        if (strcmp(exports[i].undec_name, und_fname) == 0) return exports[i].address;
    return 0;
}

CCodeLib::symbol *CCodeLib::FindSym(char *sym_name)
{
    for(int i = 0; i < exports_num; i++)
        if (strcmp(exports[i].name, sym_name) == 0) return &exports[i];
    return 0;
}

CCodeLib::symbol *CCodeLib::FindSymUndec(char *und_sym_name)
{
    for(int i = 0; i < exports_num; i++)
        if (strcmp(exports[i].undec_name, und_sym_name) == 0) return &exports[i];
    return 0;
}

int CCodeLib::operator ()(char *fn_name)
{
    if (!image_ready) return 0;
    symbol *sym = FindSymUndec(fn_name);
    if (!sym) return 0;
    if (!(sym->type & SYMBOL_TYPE_CODE)) return 0;

    void (__cdecl *func)() = (void (__cdecl *)())sym->address;
    func();
    return 1;
}
