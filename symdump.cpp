#include <stdio.h>
#include <windows.h>
#include <imagehlp.h>
#include <vector>
#include <string>
#include "resstdfile.h"
#include "codelib.h"

class CScriptContainer
{
public:
    std::vector<CCodeLib *> CodeLibs;
    std::vector<std::string> Names;
    CScriptContainer();
    int AddObject(CResFile *f, const char *name);
    int Link();
};

CScriptContainer::CScriptContainer()
{
}

int CScriptContainer::AddObject(CResFile *f, const char *name)
{
    CCodeLib *code = new CCodeLib;
    if (!code->LoadObjCode(f))
    {
        printf("failed to load: %s\n", name);
        delete code;
        return 0;
    }
    CodeLibs.push_back(code);
    Names.push_back(name);
    return 1;
}

int CScriptContainer::Link()
{
    int libs = CodeLibs.size();
    for(int i = 0; i < libs; i++)
    {
        if (CodeLibs[i]->LinkMany(&CodeLibs[0], libs)) continue;
    }
    return 1;
}

int main(int argc,char* argv[])
{
    if (argc < 2)
    {
        printf("OBJ loader\n");
        return 0;
    }
    CScriptContainer cont;
    /*CCodeLib lib;
    // -------------- Load
    cfunction funcs[]=
    {
        {"??2@YAPAXI@Z",operator new,0},
        {"??3@YAXPAX@Z",operator delete,0},
        {"_printf",printf,0},
        {"_sprintf",sprintf,0},
        {"_fopen",fopen,0},
        {"_fclose",fclose,0},
        {"_fread",fread,0},
        {"_fseek",fseek,0},
        {"_ftell",ftell,0},  
        {"__imp__MessageBoxA@16",MessageBoxA,FUNCTION_TYPE_CREATE_POINTER},
        {"__imp__UnDecorateSymbolName@16",UnDecorateSymbolName,FUNCTION_TYPE_CREATE_POINTER},
        {"__imp__VirtualAlloc@16",VirtualAlloc,FUNCTION_TYPE_CREATE_POINTER},
        {"__imp__VirtualFree@12",VirtualFree,FUNCTION_TYPE_CREATE_POINTER},
        {0,0}
    };
    lib.LoadProcs(funcs);*/

    int arg = 1;
    while(arg < argc)
    {
        CResFile *f = new CStandartResFile(argv[arg]);
        if (!f)
            return 0;
        cont.AddObject(f, argv[arg]);
        delete f;
        arg++;
    }
    cont.Link();

    int i, undec = 1;
    char buf[1024];
    int bbs_size = 0, code_size = 0, data_size = 0, rdata_size = 0;
    for(int l = 0; l < cont.CodeLibs.size(); l++)
    {
        CCodeLib *obj = cont.CodeLibs[l];

        bbs_size  += obj->GetBSS_Size();
        code_size += obj->GetCode_Size();
        data_size += obj->GetData_Size();
        rdata_size += obj->GetRData_Size();
        printf("\n%s sizes (code %d, rdata %d, data %d, bss %d):\n", cont.Names[l].c_str(),
            obj->GetCode_Size(), obj->GetRData_Size(), obj->GetData_Size(), obj->GetBSS_Size());

        printf("exports:\n");
        for(i = 0; i < obj->exports_num; i++)
        {
            if (undec)
                UnDecorateSymbolName(obj->exports[i].name, buf, 256, 0);
            else
                strcpy(buf, obj->exports[i].name);
            const char *type, *rw;
            if (obj->exports[i].type & SYMBOL_TYPE_CODE) type = "- function"; else
                if (obj->exports[i].type & SYMBOL_TYPE_INITIALIZED_DATA) type = "- initialized data"; else
                    if (obj->exports[i].type & SYMBOL_TYPE_UNINITIALIZED_DATA) type = "- uninitialized data"; else
                        type = "- unknown\n";
            rw = (obj->exports[i].type & SYMBOL_TYPE_WRITE) ? "writable" : "read-only";
            printf("(%i) %s %d %s(%s)\n", i, buf, obj->exports[i].size, type, rw);
        }

        printf("\nunresolved imports:\n");
        for(i = 0; i < obj->imports_num;i++)
        {
            if (obj->imports[i].address)
                continue;
            if (undec)
                UnDecorateSymbolName(obj->imports[i].name, buf, 256, 0);
            else
                strcpy(buf, obj->imports[i].name);
            printf("(%i) %s ", i, buf);
            if (obj->imports[i].address)
            {
                if (obj->imports[i].type & SYMBOL_TYPE_CODE) printf("- function\n"); else
                    if (obj->imports[i].type & SYMBOL_TYPE_INITIALIZED_DATA) printf("- initialized data\n"); else
                        if (obj->imports[i].type & SYMBOL_TYPE_UNINITIALIZED_DATA) printf("- uninitialized data\n"); else
                            printf("- unknown\n");
            } else printf("(unresolved)\n");
        }
    }
    printf("\ntotals: code %d, rdata %d, data %d, bss %d\n",
        code_size, rdata_size, data_size, bbs_size);

    return 0;
}
