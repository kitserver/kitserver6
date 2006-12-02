//hooklib.h

#ifndef HOOKLIB_H
#define HOOKLIB_H

#include <windows.h>
#include <map>
#include <vector>
#include <string>


using namespace std;


class hook_point {
    public:
        hook_point() { hook_point(0,0,0); }
        hook_point(DWORD call_site, DWORD numArgs, DWORD target) : 
            _call_site(call_site), 
            _numArgs(numArgs),
            _target(target)  
        {}

        DWORD _call_site;
        DWORD _numArgs;
        DWORD _target;
};

enum {
	HOOKTYPE_NONE, HOOKTYPE_CALL, HOOKTYPE_JMP, HOOKTYPE_CALLPTR,
};

class hook_manager {
    public:
        hook_manager() { InitializeCriticalSection(&_cs); }
        ~hook_manager();

        void SetCallHandler(void* addr);
        bool hook(hook_point& hp);
        bool unhook(hook_point& hp);
        DWORD getFirstTarget(DWORD call_site, bool* lastAddress=NULL);
        DWORD getNextTarget(DWORD call_site, bool* lastAddress=NULL);
        DWORD getOriginalTarget(DWORD call_site);
        DWORD getNumArgs(DWORD call_site);
        DWORD getType(DWORD call_site);
        string get_last_error();

    private:
        string _last_error; //holds info about last error
        map<DWORD,vector<DWORD>*> _hooks;
        map<DWORD,vector<DWORD>::iterator> _currCallPos;
        map<DWORD,DWORD> _numArgsMap;
        map<DWORD,DWORD> _typeMap;
        CRITICAL_SECTION _cs;
        DWORD _callHandler;

        void add_hook_point(DWORD original, hook_point& hp);
        DWORD remove_hook_point(hook_point& hp);
};

#endif //HOOKLIB_H
