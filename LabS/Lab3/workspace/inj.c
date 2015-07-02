//inj.c
#include <windows.h>
#pragma comment(lib,"user32.lib")
PBYTE pRemoteCode, pCode, pCode2, pOrignCode;
DWORD dwSizeOfCode;
//DWORD addrFunction,addrIAFuntion;

DWORD  GetIAFromImportTable(DWORD dwBase,LPCSTR lpszFuncName);
void _str_msgboxa();
void _addr_GetModuleHandleA();
void _addr_MessageBoxA();

void MyMessageBox();
// code_start�Ƕ�������Ŀ�ʼ���
__declspec(naked) void code_start()
{
    __asm {
        push ebp
        mov  ebp, esp
        push ebx
				//Local variables
        sub  esp, 0x10
        // ebp - 0x0C ===> ImageBase
				// self-locating �Զ�λ ���Ķ�����������3��ָ��ĺ���
        call _delta
_delta:
        pop  ebx
        sub  ebx, offset _delta
				// ����GetModuleHandleA()
        push 0
        lea  ecx, [ebx + _addr_GetModuleHandleA]
        call dword ptr [ecx]
        cmp  eax, 0x0
        jne  _cont1
        mov  eax, 0x1
        jmp  _ret
_cont1:
        mov  [ebp-0x0C], eax
				// ����GetIAFromImportTable();
        lea  ecx, [ebx + _str_msgboxa]
        push ecx
        push [ebp-0x0C]
        call offset GetIAFromImportTable
        add  esp, 0x8
        cmp  eax, 0x0
        jne  _ret
        mov  eax, 0x2
_ret:
        add  esp, 0x20
        pop  ebx
        mov  esp, ebp
        pop  ebp
        ret
    }
}
// _str_msgboxa���ַ�����MessageBoxA���ĵ�ַ
__declspec(naked) void _str_msgboxa()
{
    __asm {
        _emit 'M'
        _emit 'e'
        _emit 's'
        _emit 's'
        _emit 'a'
        _emit 'g'
        _emit 'e'
        _emit 'B'
        _emit 'o'
        _emit 'x'
        _emit 'A'
        _emit 0x0
    }
}
// _addr_GetModuleHandleA�Ǵ��GetModuleHandleA()��ȫ�ֱ��� ����
__declspec(naked) void _addr_GetModuleHandleA()
{
    __asm {
        _emit 0xAA
        _emit 0xBB
        _emit 0xAA
        _emit 0xEE
    }
}
// ����������GetIAFromImportTable()��������ش���

DWORD  GetIAFromImportTable(DWORD dwBase,LPCSTR lpszFuncName){
	PIMAGE_DOS_HEADER 			pDosHeader;
  PIMAGE_NT_HEADERS 			pNtHeaders;
  PIMAGE_FILE_HEADER 			pFileHeader;
  PIMAGE_OPTIONAL_HEADER32 	pOptHeader;
  DWORD dwRVAImpTBL;
  DWORD dwSizeOfImpTBL;
  PIMAGE_IMPORT_DESCRIPTOR pImpTBL,p;
  PIMAGE_IMPORT_BY_NAME pOrdName;
  DWORD dwRVAName;
  PIMAGE_THUNK_DATA thunkTargetFunc;
  WORD hint;
  
  
  DWORD dwIA=0;
  pDosHeader = (PIMAGE_DOS_HEADER)dwBase;
  pNtHeaders = (PIMAGE_NT_HEADERS)(dwBase + pDosHeader->e_lfanew);
  pOptHeader = &(pNtHeaders->OptionalHeader);
  dwRVAImpTBL = pOptHeader->DataDirectory[1].VirtualAddress;
  dwSizeOfImpTBL = pOptHeader->DataDirectory[1].Size;
  pImpTBL = (PIMAGE_IMPORT_DESCRIPTOR)(dwBase + dwRVAImpTBL);
  for (p = pImpTBL; (DWORD)p < ((DWORD)pImpTBL + dwSizeOfImpTBL); p++){
    	//dwRVAName=p->Name;//dwRVAName�Ǹ���������name���ԣ�ΪRVA
    	thunkTargetFunc=(PIMAGE_THUNK_DATA)GetIAFromImpDesc(dwBase,lpszFuncName,p);
    	if(!thunkTargetFunc){
    		continue;
    		}
    		else{
    			//printf("0x%08x ==> 0x%08x\n",&(thunkTargetFunc->u1.Function),thunkTargetFunc->u1.Function);
    			//addrIAFuntion=thunkTargetFunc->u1.Function;//Function��ֵ������ڵ�ַ
    			//addrFunction=&(thunkTargetFunc->u1.Function);//IAT����function���ڵĵ�ַ
    			//return thunkTargetFunc;
    			return &(thunkTargetFunc->u1.Function);
    			}	
    }
    
    return 0;
	}
	
	
	
DWORD GetIAFromImpDesc(DWORD dwBase, LPCSTR lpszName,PIMAGE_IMPORT_DESCRIPTOR pImpDesc) 
{
    PIMAGE_THUNK_DATA pthunk, pthunk2;
    PIMAGE_IMPORT_BY_NAME pOrdinalName;
    if (pImpDesc->Name == 0) return 0;
    pthunk = (PIMAGE_THUNK_DATA) (dwBase + pImpDesc->OriginalFirstThunk);
    pthunk2 = (PIMAGE_THUNK_DATA) (dwBase + pImpDesc->FirstThunk);
    for (; pthunk->u1.Function != 0; pthunk++, pthunk2++) {
        if (pthunk->u1.Ordinal & 0x80000000) continue;//��������ֵ������
        pOrdinalName = (PIMAGE_IMPORT_BY_NAME) (dwBase + pthunk->u1.AddressOfData);//AddressOfDataָ��imageImportByName�ṹ��RVA
        if (CompStr((LPSTR)lpszName, (LPSTR)&pOrdinalName->Name)) 
            return (DWORD)pthunk2;
    }
    return 0;
}
BOOL CompStr(LPSTR s1, LPSTR s2)
{
    PCHAR p, q;
    for (p = s1, q = s2; (*p != 0) && (*q != 0); p++, q++) {
        if (*p != *q) return FALSE;
    }
    return TRUE;
}

// code_end�Ƕ�������Ľ������
__declspec(naked) void code_end()
{
    __asm _emit 0xCC
}
__declspec(naked) void MyMessageBoxA()
{
    __asm {
        push ebp
        mov  ebp, esp
        push ebx
        
        call _delta2
_delta2:
        pop ebx
        sub ebx, offset _delta2
        
        push [ebp + 0x14]
        push [ebp + 0x10]
        lea  ecx, [ebx + _str_hacked]
        push ecx
        push [ebp + 0x08]
        lea  eax, [ebx + _addr_MessageBoxA]   //need relocation
        call dword ptr [eax]
        pop  ebx
        mov  esp, ebp
        pop  ebp
        ret  16
_str_hacked:
        _emit 'I'
        _emit '\''
        _emit 'm'
        _emit ' '
        _emit 'h'
        _emit 'a'
        _emit 'c'
        _emit 'k'
        _emit 'e'
        _emit 'd'
        _emit '!'
        _emit 0x0
   }
}

__declspec(naked) void _addr_MessageBoxA()
{
    __asm
    {
        _emit 0xAA
        _emit 0xBB
        _emit 0xAA
        _emit 0xDD
    }
}
// make_code()�����ǽ���ʼ��Ǻͽ������֮������ж��������ݿ�����һ����������
DWORD make_code()
{
    int off; 
    DWORD func_addr;
    HMODULE hModule;
    __asm {
        mov edx, offset code_start
        mov dword ptr [pOrignCode], edx
        mov eax, offset code_end
        sub eax, edx
        mov dword ptr [dwSizeOfCode], eax
    }
    pCode = VirtualAlloc(NULL, dwSizeOfCode, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (pCode== NULL) {
        printf("[E]: VirtualAlloc failed\n");
        return 0;
    }
    printf("[I]: VirtualAlloc ok --> at 0x%08x.\n", pCode);
    for (off = 0; off<dwSizeOfCode; off++) {
        *(pCode+off) = *(pOrignCode+off);
    }
    printf("[I]: Copy code ok --> from 0x%08x to 0x%08x with size of 0x%08x.\n", 
        pOrignCode, pCode, dwSizeOfCode);
    hModule = LoadLibrary("kernel32.dll");
    if (hModule == NULL) {
        printf("[E]: kernel32.dll cannot be loaded. \n");
        return 0;
    }
    func_addr = (DWORD)GetProcAddress(hModule, "GetModuleHandleA");
    if (func_addr == 0) {
        printf("[E]: GetModuleHandleA not found. \n");
        return 0;
    }
    off = (DWORD)pCode - (DWORD)pOrignCode;
    *(PDWORD)((PBYTE)_addr_GetModuleHandleA + off) = func_addr;//����GetModuleHandleA
    return dwSizeOfCode;
}


// inject_code()�����Ǵ����pCode��ָ��Ļ������еĶ����ƴ���ע�뵽Զ�̽�����
int inject_code(DWORD pid)
{
    DWORD sizeInjectedCode,sizeInjectedCode2;
    DWORD hproc,hthread;
    DWORD addrMsgBoxA,enterAddrMsgBoxA;
    int rcode,TID,num,old,rcode2;
    
    sizeInjectedCode=make_code();//��������벢�������µ����򣬷��ش��볤��
    hproc = OpenProcess(
          PROCESS_CREATE_THREAD  | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION   | PROCESS_VM_WRITE 
        | PROCESS_VM_READ, FALSE, pid);//����Զ�̽��̾��
        
    rcode=(PBYTE)VirtualAllocEx(hproc,0,sizeInjectedCode,MEM_COMMIT,PAGE_EXECUTE_READWRITE);//open the space in remote process
		if(!WriteProcessMemory(hproc, rcode, pCode, sizeInjectedCode, &num)){//д�����
			printf("[E]Write pCode Failed\n");
			}else 
			{
			printf("[I]Write pCode Success\n");
			}
		hthread = CreateRemoteThread(hproc,NULL, 0, (LPTHREAD_START_ROUTINE)rcode,0, 0 , &TID);//�����߳�
		WaitForSingleObject(hthread, 0xffffffff);//�첽�ȴ�
		if(!GetExitCodeThread(hthread, &addrMsgBoxA)){//��ȡ����ֵ
  		printf("[E]Get exitCode Failed\n");
  		}else 
  		{
  		printf("[I]Get exitCode Success\n");
  		}
		printf("The address of MsgBoxA is 0x%08x\n",addrMsgBoxA);//��ӡ
		
		//��ȡaddrMsgBoxA��ַ�е�ֵ������ȡ���е�MsgBoxA����ڵ�ַ
		if(!ReadProcessMemory(hproc, addrMsgBoxA, &enterAddrMsgBoxA, 4, &num)){//д�����
			printf("[E]Read Addr Failed\n");
			}else 
			{
			printf("[I]Read Addr Success\n");
			}
		//����ڵ�ַ���������ڴ浥Ԫ
		 if(!VirtualProtect(_addr_MessageBoxA, 4, PAGE_EXECUTE_READWRITE, &old)){
		//�޸Ĵ����Ϊ��д
			printf("[E]Change to be Writable Failed\n");
		}else 
		{
			printf("[I]Change to be Writable Success\n");
		}
		*(PDWORD)((PBYTE)_addr_MessageBoxA)=enterAddrMsgBoxA;//����β���д��������˼
		pCode2=(PBYTE)MyMessageBoxA;
		
		rcode2=(PBYTE)VirtualAllocEx(hproc,0,200,MEM_COMMIT,PAGE_EXECUTE_READWRITE);//open the space in remote process ���ڲ�ȷ���Ƕδ�����ٳ���Ϊ��͵����������Զ���ڴ���ʵ�ʳ��ȵ�ֵ200������Ӧ��û����
		if(!WriteProcessMemory(hproc, rcode2,pCode2, 200, &num)){//д����� num�������ʵ�ʳ���
			printf("[E]Write pCode2 Failed\n");
			}else 
			{
			printf("[I]Write pCode2 Success\n");
			}
		//�޸�addrMsgBoxA�ڴ浥Ԫ�ڵ�ֵ
		if(!VirtualProtectEx( hproc,addrMsgBoxA, 4, PAGE_EXECUTE_READWRITE, &old)){ 
		//�޸Ĵ����Ϊ��д
			printf("[E]Change to be Writable Failed\n");
		}else 
		{
			printf("[I]Change to be Writable Success\n");
		}
		if(!WriteProcessMemory(hproc, addrMsgBoxA,&rcode2, 4, &num)){//д����� rcodeҪȡ��ַ�������޷�������е�ֵ
			printf("[E]Write pCode2 Failed\n");
			}else 
			{
			printf("[I]Write pCode2 Success\n");
			}
		
			
    return 0;
}
int main(int argc, char *argv[])
{
    DWORD pid = 0;
    // Ϊpid��ֵΪhello.exe�Ľ���ID
    if (argc < 2) {
        printf("Usage: %s pid\n", argv[0]);
        return -1;
    }
    pid = atoi(argv[1]);
		if (pid <= 0) {
        	printf("[E]: pid must be positive (pid>0)!\n"); 
        	return -2;
		}  
    
    inject_code(pid);
    return 0;
  }