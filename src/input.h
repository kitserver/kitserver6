#ifndef _INPUT_H_
#define _INPUT_H_

#include <windows.h>

#ifndef KEXPORT
#define KEXPORT EXTERN_C __declspec(dllexport)
#endif

// input dwords
#define DIRECTIONAL_PRESSED 0
#define DIRECTIONAL_RELEASED 8 
#define FUNCTIONAL 16

// functional input bitmasks
#define CROSS_PRESSED  0x01
#define TRIANGLE_PRESSED 0x02
#define SQUARE_PRESSED  0x04
#define CIRCLE_PRESSED 0x08
#define L1_PRESSED 0x100
#define R1_PRESSED 0x200
#define L2_PRESSED 0x1000
#define R2_PRESSED 0x2000
#define SELECT_PRESSED 0x10000
#define START_PRESSED 0x400000

// directions input bitmasks
#define UP_PRESSED 0x10
#define DOWN_PRESSED 0x20
#define LEFT_PRESSED 0x40
#define RIGHT_PRESSED 0x80

// check macros
#define INPUT_EVENT(table,controller,type,x) (table[type+controller] & x)

void HookGameInput(DWORD hook_cs, DWORD inputTableAddr);
void UnhookGameInput(DWORD hook_cs);
KEXPORT DWORD* GetInputTable();

#endif
