#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
typedef LONG (*StatusFn)(void);
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    HMODULE dll=LoadLibraryW(argv[1]);
    if(!dll){printf("LoadLibrary failed: %lu\n",GetLastError());return 3;}
    StatusFn status=(StatusFn)GetProcAddress(dll,"SendToStash_GetStatus");
    if(!status)return 4;
    for(int i=0;i<100 && status()==0;++i)Sleep(50);
    LONG result=status();
    printf("Host guard status=%ld; expected 1 (unsupported host, hooks disabled)\n",result);
    /* Process exits with DLL resident, like an ASI loader. */
    return result==1 ? 0 : 5;
}
