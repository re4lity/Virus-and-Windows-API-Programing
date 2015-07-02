#include <windows.h>
#include <stdio.h>
#define MAX_SECTION_NUM   16
#define MAX_IMPDESC_NUM   64

HANDLE  hHeap;
PIMAGE_DOS_HEADER	pDosHeader;
PCHAR   pDosStub;
DWORD   dwDosStubSize;
DWORD   dwDosStubOffset;
PIMAGE_NT_HEADERS           pNtHeaders;
PIMAGE_FILE_HEADER          pFileHeader;
PIMAGE_OPTIONAL_HEADER32    pOptHeader;
PIMAGE_SECTION_HEADER   pSecHeaders;
PIMAGE_SECTION_HEADER   pSecHeader[MAX_SECTION_NUM];
WORD  wSecNum;//�θ���
PBYTE pSecData[MAX_SECTION_NUM];
PBYTE pCode2, pOrignCode;
DWORD dwSecSize[MAX_SECTION_NUM];
DWORD dwFileSize;
DWORD originAOEP;
WORD  wInjSecNo;
DWORD dwSizeOfCode = 0x4;
BYTE pCode[] ={0xCC, 0xCC, 0xCC, 0xCC};
void _addr_orign_aoep();

__declspec(naked) void code_start()
{
    __asm {
        pushad															//push EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
        call    _delta
_delta:
        pop     ebx                         // save registers context from stack
        sub     ebx, offset _delta

        mov     eax, 0x00400000
        add     eax,dword ptr [ebx + _addr_orign_aoep]
        mov     dword ptr [esp + 0x1C], eax     // save (dwBaseAddress + orign) to eax-location-of-context
        popad                                   // load registers context from stack
        push    eax
        xor     eax, eax
        retn                                    // >> jump to the Original AOEP
  }
}
__declspec(naked) void _addr_orign_aoep()
{
    __asm {
        _emit   0xCC
        _emit   0xCC
        _emit   0xCC
        _emit   0xCC
    }
}
__declspec(naked) void code_end()
{
    __asm _emit 0xCC
}

DWORD make_code()
{
    int off; 
    __asm {
        mov edx, offset code_start
        mov dword ptr [pOrignCode], edx
        mov eax, offset code_end
        sub eax, edx
        mov dword ptr [dwSizeOfCode], eax
    }
    pCode2 = VirtualAlloc(NULL, dwSizeOfCode, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (pCode2== NULL) {
        printf("[E]: VirtualAlloc failed\n");
        return 0;
    }
    printf("[I]: VirtualAlloc ok --> at 0x%08x.\n", pCode2);
    for (off = 0; off<dwSizeOfCode; off++) {
        *(pCode2+off) = *(pOrignCode+off);
    }
    printf("[I]: Copy code ok --> from 0x%08x to 0x%08x with size of 0x%08x.\n", 
        pOrignCode, pCode2, dwSizeOfCode);
    off = (DWORD)pCode2 - (DWORD)pOrignCode;
    *(PDWORD)((PBYTE)_addr_orign_aoep + off) = originAOEP;//����AddressOfEntryPoint
    return dwSizeOfCode;
}
static DWORD PEAlign(DWORD dwTarNum,DWORD dwAlignTo)
{	
    return (((dwTarNum+dwAlignTo - 1) / dwAlignTo) * dwAlignTo);
}

static DWORD RVA2Ptr(DWORD dwBaseAddress, DWORD dwRva)//RVAƫ��ָ�� ת���� PTRָ�루����Ϊ��ַ��RVA��
{
    if ((dwBaseAddress != 0) && dwRva)
        return (dwBaseAddress + dwRva);
    else
        return dwRva;
}

//----------------------------------------------------------------
static PIMAGE_SECTION_HEADER RVA2Section(DWORD dwRVA)//RVAƫ��ָ�� ת���� ӡ��ε�ͷ�� ������ΪRVA��
{
    int i;
    for(i = 0; i < wSecNum; i++) {//�������еĶ�
        if ( (dwRVA >= pSecHeader[i]->VirtualAddress)&& (dwRVA <= (pSecHeader[i]->VirtualAddress+ pSecHeader[i]->SizeOfRawData)) ) {//���ڴ�ƫ���ڸö����ڴ��������ַ��Χ�ڣ��򷵻ظö�
            return ((PIMAGE_SECTION_HEADER)pSecHeader[i]);
        }
    }
    return NULL;
}

//----------------------------------------------------------------
static PIMAGE_SECTION_HEADER Offset2Section(DWORD dwOffset)//Offsetʵ�ʴ���ƫ�Ƶ�ַ ת���� ��ͷ��
{
    int i;
    for(i = 0; i < wSecNum; i++) {
        if( (dwOffset>=pSecHeader[i]->PointerToRawData) 
            && (dwOffset<(pSecHeader[i]->PointerToRawData + pSecHeader[i]->SizeOfRawData)))//������ƫ���ڸö���������ַ��Χ�ڣ��򷵻ظö�
        {
            return ((PIMAGE_SECTION_HEADER)pSecHeader[i]);
        }
    }
    return NULL;
}

//================================================================
static DWORD RVA2Offset(DWORD dwRVA)//RVA ת���� Offset �ڴ��ַ�����̵�ַ
{
    PIMAGE_SECTION_HEADER pSec;
    pSec = RVA2Section(dwRVA);//ImageRvaToSection(pimage_nt_headers,Base,dwRVA); ͨ��rva���ҽ�ͷ��
    if(pSec == NULL) {
        return 0;
    }
    return (dwRVA + (pSec->PointerToRawData) - (pSec->VirtualAddress));//�����ַ-��������ʼ��ַ+��ԭʼ������ʼ��ַ=��Ӧ���̵�ַ
}
//----------------------------------------------------------------
static DWORD Offset2RVA(DWORD dwOffset)// Offsetת����RVA  ���̵�ַ���ڴ��ַ
{
    PIMAGE_SECTION_HEADER pSec;
    pSec = Offset2Section(dwOffset);//ͨ��offset����ͷ��
    if(pSec == NULL) {
        return (0);
    }
    return(dwOffset + (pSec->VirtualAddress) - (pSec->PointerToRawData));//���̵�ַ-��ԭʼ������ʼ��ַ+��������ʼ��ַ=��Ӧ�����ַ
}

BOOL CopyPEFileToMem(LPCSTR lpszFilename)//����Ӧ�ļ����Ŀ�ִ���ļ������ڴ���з���
{
    HANDLE  hFile;

    PBYTE   pMem;
    DWORD   dwBytesRead;
    int     i;

    DWORD   dwSecOff;

    PIMAGE_NT_HEADERS       pMemNtHeaders;  	//�ڴ��е�PEͷ
    PIMAGE_SECTION_HEADER   pMemSecHeaders;		//�ڴ��еĽ�ͷ��

    hFile = CreateFile(lpszFilename, GENERIC_READ,
                FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);//���ļ�

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[E]: Open file (%s) failed.\n", lpszFilename);
        return FALSE;
    }
    dwFileSize = GetFileSize(hFile, 0);//�²�0����64λ����ϵͳ
    printf("[I]: Open file (%s) ok, with size of 0x%08x.\n", lpszFilename, dwFileSize);
    if (hHeap == NULL) { 
        printf("[I]: Get the default heap of the process. \n");
        hHeap = GetProcessHeap();
    }//��ý��̵Ķ�
    if (hHeap == NULL) {
        printf("[E]: Get the default heap failed.\n");
        return FALSE;
    }

    pMem = (PBYTE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwFileSize);//�ڶ�����Ϊ�ļ�����һ�������Ŵ��������ļ�

    if(pMem == NULL) {
        printf("[E]: HeapAlloc failed (with the size of 0x%08x).\n", dwFileSize);
        CloseHandle(hFile);
        return FALSE;
    }
  
    ReadFile(hFile, pMem, dwFileSize, &dwBytesRead, NULL);//���ļ���������
    
    CloseHandle(hFile);

    // Copy DOS Header 
    pDosHeader = (PIMAGE_DOS_HEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(IMAGE_DOS_HEADER));//ͷ�ļ��а����Ľṹ��dosͷ��С�������

    if(pDosHeader == NULL) {
        printf("[E]: HeapAlloc failed for DOS_HEADER\n");
        CloseHandle(hFile);
        return FALSE;
    }

    CopyMemory(pDosHeader, pMem, sizeof(IMAGE_DOS_HEADER));//��DOSͷ������

    // Copy DOS Stub Code 
    dwDosStubSize = pDosHeader->e_lfanew - sizeof(IMAGE_DOS_HEADER);//e_lfanew��PEͷ���ļ��е�ƫ�ƣ�DOSͷ�Ĵ�С����dos stub����ʼ��ַ
    dwDosStubOffset = sizeof(IMAGE_DOS_HEADER);
    pDosStub = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwDosStubSize);
    if ((dwDosStubSize & 0x80000000) == 0x00000000)//���Ǵ���dos stub�����俽���ڴ�
    {
        CopyMemory(pDosStub, (const void *)(pMem + dwDosStubOffset), dwDosStubSize);
    }

    // Copy NT HEADERS 
    pMemNtHeaders = (PIMAGE_NT_HEADERS)(pMem + pDosHeader->e_lfanew);
    
    pNtHeaders = (PIMAGE_NT_HEADERS)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(IMAGE_NT_HEADERS));

    if(pNtHeaders == NULL) {
        printf("[E]: HeapAlloc failed for NT_HEADERS\n");
        CloseHandle(hFile);
        return FALSE;
    }

    CopyMemory(pNtHeaders, pMemNtHeaders, sizeof(IMAGE_NT_HEADERS));//���ļ��е�windowsͷ����һ�������з���

    pOptHeader = &(pNtHeaders->OptionalHeader);//��ѡͷ
    pFileHeader = &(pNtHeaders->FileHeader);//�ļ�ͷ ����ʱ�����������Ȼ�������ֵ����ȥ�ڴ��е�pe�ļ��в��Ҷ�Ӧֵ

    // Copy SectionTable
    pMemSecHeaders = (PIMAGE_SECTION_HEADER) ((DWORD)pMemNtHeaders + sizeof(IMAGE_NT_HEADERS));
    wSecNum = pFileHeader->NumberOfSections;

    pSecHeaders = (PIMAGE_SECTION_HEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, wSecNum * sizeof(IMAGE_SECTION_HEADER));

    if(pSecHeaders == NULL) {
        printf("[E]: HeapAlloc failed for SEC_HEADERS\n");
        CloseHandle(hFile);
        return FALSE;
    }

    CopyMemory(pSecHeaders, pMemSecHeaders, wSecNum * sizeof(IMAGE_SECTION_HEADER));

    for(i = 0; i < wSecNum; i++) {
        pSecHeader[i] = (PIMAGE_SECTION_HEADER) 
          ((DWORD)pSecHeaders + i * sizeof(IMAGE_SECTION_HEADER));
    }

    // Copy Section
    for(i = 0; i < wSecNum; i++) {
        dwSecOff = (DWORD)(pMem + pSecHeader[i]->PointerToRawData);
        
        dwSecSize[i] = PEAlign(
         pSecHeader[i]->SizeOfRawData, pOptHeader->FileAlignment);
        
        pSecData[i] = (PBYTE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwSecSize[i]);

        if (pSecData[i] == NULL) {
            printf("[E]: HeapAlloc failed for the section of %d\n", i);
            CloseHandle(hFile);
            return FALSE;
        }

        CopyMemory(pSecData[i], (PVOID)dwSecOff, dwSecSize[i]);
    }

    HeapFree(hHeap, 0, pMem);
    printf("[I]: Load PE from file (%s) ok\n", lpszFilename);

    return TRUE;
}

void DumpPEInMem()
{
    // ��������������Ĵ���
    int i;
    PIMAGE_SECTION_HEADER p;
    //��ӡImageBase�ֶ�
    printf("ImageBase in ImageOptHeader: 0x%08x\n",pOptHeader->ImageBase);
    //��ӡAddressOfEntryPoint
    originAOEP= pOptHeader->AddressOfEntryPoint;
    printf("AddressOfEntryPoint in ImageOptHeader: 0x%08x\n",(pOptHeader->ImageBase+pOptHeader->AddressOfEntryPoint));
    //��ӡNumberOfSections
    printf("NumberOfSections in ImageFileHeader: 0x%08x\n",pFileHeader->NumberOfSections);
    //��ӡSizeOfImage
    printf("SizeOfImage in ImageOptHeader: 0x%08x\n",pOptHeader->SizeOfImage);
    
    //��ӡsectionTable
    printf("SectionTable:\n");
    printf("Name\t VirtualSize\t VirtualAddress\t RawDataSize\t RawDataOffset\t Characteristics\n");
    for(i=0;i<wSecNum;i++){
    	p=(PIMAGE_SECTION_HEADER)pSecData[i];
    	printf("0x%08x\t ",p->Name);
    	printf("0x%08x\t ",p->VirtualAddress);
    	printf("0x%08x\t ",p->SizeOfRawData);
    	printf("0x%08x\t ",p->PointerToRawData);
    	printf("0x%08x",p->Characteristics);
    	printf("\n");
    	}
    return;
}

BOOL AddNewSection()//����һ���ڵ�ͷ���Լ�������
{
    DWORD roffset,rsize,voffset,vsize;//roffset=rawOffset vOffset=virtualOffset
    DWORD dwOffsetOfcode;//�����ʵ��ƫ��
    PIMAGE_SECTION_HEADER pInjSecHeader;//ע��Ľ�ͷ��
    int   i;

    wInjSecNo = wSecNum;

    if (wInjSecNo >= 64) {
        printf("[E]: Too many sections, wInjSecNo(%d) >= 63\n", wInjSecNo);
        return FALSE;
    }

    pInjSecHeader = (PIMAGE_SECTION_HEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(IMAGE_SECTION_HEADER));//�ڶ������һ����������Žڵ�ͷ
    if(pInjSecHeader == NULL) {
        printf("[E]: HeapAlloc failed for NEW_SEC_HEADER\n");
        return FALSE;
    }
    pSecHeader[wInjSecNo] = pInjSecHeader;

    // ���������
    // rsize = �µĽڵĳߴ磬����FileAlignment����;
    rsize=PEAlign(dwSizeOfCode,pOptHeader->FileAlignment);

    // ���������
    // vsize = �µĽڵĳߴ磬����SectionAlignment����;
		vsize=PEAlign(dwSizeOfCode,pOptHeader->SectionAlignment);

    roffset = PEAlign(pSecHeader[wInjSecNo-1]->PointerToRawData + pSecHeader[wInjSecNo-1]->SizeOfRawData,
            pOptHeader->FileAlignment);

    voffset = PEAlign(pSecHeader[wInjSecNo-1]->VirtualAddress + pSecHeader[wInjSecNo-1]->Misc.VirtualSize,
           pOptHeader->SectionAlignment);

    pSecHeader[wInjSecNo]->PointerToRawData     = roffset;
    pSecHeader[wInjSecNo]->VirtualAddress       = voffset;
    pSecHeader[wInjSecNo]->SizeOfRawData        = rsize;
    pSecHeader[wInjSecNo]->Misc.VirtualSize     = vsize;
    pSecHeader[wInjSecNo]->Characteristics      = 0x60000020;
    pSecHeader[wInjSecNo]->Name[0] = '.';
    pSecHeader[wInjSecNo]->Name[1] = 'm';
    pSecHeader[wInjSecNo]->Name[2] = 'y';
    pSecHeader[wInjSecNo]->Name[3] = 'c';
    pSecHeader[wInjSecNo]->Name[4] = 'o';
    pSecHeader[wInjSecNo]->Name[5] = 'd';
    pSecHeader[wInjSecNo]->Name[6] = 'e';
    pSecHeader[wInjSecNo]->Name[7] = '.';
    
    pSecData[wInjSecNo] = (PBYTE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, rsize);
    dwSecSize[wInjSecNo] = pSecHeader[wInjSecNo]->SizeOfRawData;//�ڽڴ�С������������һ��
    //CopyMemory(pSecData[wInjSecNo], pCode, dwSizeOfCode);//�������� ����1 2
    CopyMemory(pSecData[wInjSecNo], pCode2, dwSizeOfCode);//��������
    wSecNum++;
    pFileHeader->NumberOfSections = wSecNum;//��д����������
    for(i = 0;i< wSecNum; i++) {
        pSecHeader[i]->VirtualAddress =
            PEAlign(pSecHeader[i]->VirtualAddress, pOptHeader->SectionAlignment);//�ڴ���ƫ�Ƶ�ַ����
        pSecHeader[i]->Misc.VirtualSize =
            PEAlign(pSecHeader[i]->Misc.VirtualSize, pOptHeader->SectionAlignment);//�ڴ��ڽڴ�С����
        
        pSecHeader[i]->PointerToRawData =
            PEAlign(pSecHeader[i]->PointerToRawData, pOptHeader->FileAlignment);//�ļ���ƫ�Ƶ�ַ����
        
        pSecHeader[i]->SizeOfRawData = 
            PEAlign(pSecHeader[i]->SizeOfRawData, pOptHeader->FileAlignment);//�ļ��ڽڴ�С����
    }
    pOptHeader->SizeOfImage = pSecHeader[wSecNum-1]->VirtualAddress + pSecHeader[wSecNum-1]->Misc.VirtualSize;//�½��ڴ��ڵĴ�С
    dwFileSize = pSecHeader[wSecNum-1]->PointerToRawData + pSecHeader[wSecNum - 1]->SizeOfRawData;//�½��ļ��ڴ�С
    pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;//�Ͷ�̬������صı�������
    pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
    return TRUE;
}

BOOL SaveMemToPEFile(LPCSTR lpszFileName)
{
    DWORD   dwBytesWritten = 0;
    PBYTE   pMem = NULL;
    PBYTE   pMemSecHeaders;
    HANDLE  hFile;
    int i;

		
    //----------------------------------------
    pMem = (PBYTE) HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwFileSize);//���ڴ��д���һ�����ļ��Ļ�����
    if(pMem == NULL) {
        return FALSE;
    }
    //----------------------------------------
    CopyMemory(pMem, pDosHeader, sizeof(IMAGE_DOS_HEADER));//�Ƚ�dosͷ������ȥ
    if((dwDosStubSize & 0x80000000) == 0x00000000) {
        CopyMemory(pMem + dwDosStubOffset, pDosStub, dwDosStubSize);//����dosStub�Ϳ���
    }
    pOptHeader->AddressOfEntryPoint=(DWORD)pSecHeader[wSecNum-1]->VirtualAddress;//�޸Ĵ������ڵ�ַ
    CopyMemory(pMem + pDosHeader->e_lfanew,
        pNtHeaders,
        sizeof(IMAGE_NT_HEADERS));

    pMemSecHeaders = pMem + pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);//��windowsͷ��������ȥ

    //----------------------------------------
    for (i = 0; i < wSecNum; i++) {
        CopyMemory(pMemSecHeaders + i * sizeof(IMAGE_SECTION_HEADER),
        pSecHeader[i], sizeof(IMAGE_SECTION_HEADER));
    }

    for (i = 0; i < wSecNum; i++) {
        CopyMemory(pMem + pSecHeader[i]->PointerToRawData,
        pSecData[i],
        pSecHeader[i]->SizeOfRawData);
    }
    
    printf("[I]: The pieces of PE have been rearranged. \n");

    hFile = CreateFile(lpszFileName,
            GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        printf("[E]: Create file (%s) failed\n", lpszFileName);
        return FALSE;
    }
    printf("[I]: Create file (%s) ok\n", lpszFileName);
  

    // ----- WRITE FILE MEMORY TO DISK -----
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    WriteFile(hFile, pMem, dwFileSize, &dwBytesWritten, NULL);

    // ------ FORCE CALCULATED FILE SIZE ------
    SetFilePointer(hFile, dwFileSize, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    CloseHandle(hFile);
    //----------------------------------------

    HeapFree(hHeap, 0, pMem);
    
    printf("[I]: Save PE to file (%s) ok\n", lpszFileName);
    return TRUE;
}
int main()
{
    LPCSTR lpszFileName = "hello.exe";
    LPCSTR lpszInjFileName = "hello_inj.exe";

    if (! CopyPEFileToMem(lpszFileName)) {
        return -1;
    }
    //��һ��������PE�ļ�
    DumpPEInMem();
    //�ڶ������������
    dwSizeOfCode= make_code();
    //��������������д��ڱ��������µĽ�
    AddNewSection();
    //���Ĳ������½��ڴ��еĴ��뱣�浽�ļ���
    SaveMemToPEFile(lpszInjFileName);
    return 0;
}