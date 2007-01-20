//hooktest.cpp

#include "hooklib.h"
#include <cstdlib>
#include <stdio.h>

using namespace std;
hook_manager manager;
DWORD lastCallSite=0;
//sample code
BYTE code[] = {
    0xe8, 0x1, 0x2, 0x3, 0x4
};

void CallFirst(...)
{	
	DWORD oldEBP;
	__asm mov oldEBP, ebp
	
	DWORD* arg=(DWORD*)oldEBP+2;
	//this would be replaced with a function to get the call site
	DWORD call_site=(DWORD)code;
	DWORD addr=manager.getFirstTarget(call_site);
	DWORD before=lastCallSite;
	lastCallSite=call_site;
	if (addr==0) return;
	DWORD numArgs=manager.getNumArgs(call_site);
	
	//writing this as inline assembler allows to
	//give as much parameters as we went and more
	//important, we can restore all registers
	for (int i=0;i<numArgs;i++) {
		__asm mov eax, arg
		__asm mov eax, [eax]
		__asm push eax
		arg++;
	};
	
	__asm call ds:[addr]
	
	lastCallSite=before;
	
	return;
};

void CallNext(...)
{	
	DWORD oldEBP;
	__asm mov oldEBP, ebp
	
	if (lastCallSite==0) return;
	
	DWORD* arg=(DWORD*)oldEBP+2;
	DWORD addr=manager.getNextTarget(lastCallSite);
	if (addr==0) return;
	DWORD numArgs=manager.getNumArgs(lastCallSite);

	for (int i=0;i<numArgs;i++) {
		__asm mov eax, arg
		__asm mov eax, [eax]
		__asm push eax
		arg++;
	};

	__asm call ds:[addr]
	
	return;
};

int fOriginal(DWORD p1) {
	printf("This is the original function ""code"" pointed to. (%d)\n",p1);
	return 0;
}

int fA(DWORD p1) {
    printf("fA() called (%d)\n",p1);
    CallNext(999);
    printf("fA() exit\n");
    return 0;
}
int fB(DWORD p1) {
    printf("fB() called (%d)\n",p1);
    CallNext(789);
    printf("fB() exit\n");
    return 0;
}
int fC(DWORD p1) {
    printf("fC() called (%d)\n",p1);
    CallNext(456);
    printf("fC() exit\n");
    return 0;
}

void print_code(BYTE* code)
{
    printf("code: %02x %02x %02x %02x %02x\n",
            code[0],code[1],code[2],code[3],code[4]);
}


/**
 * Unit-test for hooklib library
 */
 int main(int argc, char** argv)
{
    DWORD* ptr=(DWORD*)(code+1);
    ptr[0]=(DWORD)fOriginal-((DWORD)code+5);
    
    print_code(code);

    hook_point hp1,hp2;

    hp1._call_site = (DWORD)code;
    hp1._target = (DWORD)fA;
    hp1._numArgs = 1;
    manager.hook(hp1);
    print_code(code);

    hp2._call_site = (DWORD)code;
    hp2._target = (DWORD)fB;
    hp1._numArgs = 1;
    manager.hook(hp2);
    print_code(code);

    hook_point hp3((DWORD)code, (DWORD)fC, 1);
    manager.hook(hp3);
    print_code(code);
	
	CallFirst(123);

    system("pause");

    return 0;
}
