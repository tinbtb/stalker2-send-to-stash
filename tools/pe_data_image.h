/* Test helper: never grants executable memory permissions. */
#pragma once
static unsigned char *load_data(const wchar_t *path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return NULL;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(file,&size) || size.QuadPart<4096 || size.QuadPart>0x40000000){CloseHandle(file);return NULL;}
    HANDLE mapping=CreateFileMappingW(file,NULL,PAGE_READONLY,0,0,NULL);
    CloseHandle(file);
    if(!mapping)return NULL;
    unsigned char *raw=MapViewOfFile(mapping,FILE_MAP_READ,0,0,0),*image=NULL;
    CloseHandle(mapping);
    if(!raw)return NULL;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)raw;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
       (uint64_t)dos->e_lfanew+sizeof(IMAGE_NT_HEADERS64)>(uint64_t)size.QuadPart)goto done;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(raw+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
       nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) ||
       !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96 ||
       nt->OptionalHeader.SizeOfImage>0x40000000 || nt->OptionalHeader.SizeOfHeaders>nt->OptionalHeader.SizeOfImage ||
       nt->OptionalHeader.SizeOfHeaders>(uint64_t)size.QuadPart)goto done;
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    if((unsigned char *)(sections+nt->FileHeader.NumberOfSections)>raw+size.QuadPart)goto done;
    image=VirtualAlloc(NULL,(SIZE_T)nt->OptionalHeader.SizeOfImage+0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!image)goto done;
    memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++) {
        IMAGE_SECTION_HEADER s=sections[i];
        if((uint64_t)s.PointerToRawData+s.SizeOfRawData>(uint64_t)size.QuadPart ||
           (uint64_t)s.VirtualAddress+s.SizeOfRawData>nt->OptionalHeader.SizeOfImage) {
            VirtualFree(image,0,MEM_RELEASE);image=NULL;goto done;
        }
        memcpy(image+s.VirtualAddress,raw+s.PointerToRawData,s.SizeOfRawData);
    }
done:
    UnmapViewOfFile(raw);return image;
}
