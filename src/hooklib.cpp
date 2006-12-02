//hooklib.cpp

#include "hooklib.h"

#include <windows.h>
#include <algorithm>
#include <map>
#include <vector>
#include <string>

using namespace std;

void hook_manager::SetCallHandler(void* addr)
{
	_callHandler=(DWORD)addr;
	return;
};

bool hook_manager::hook(hook_point& hp)
{
    bool result = false;
    EnterCriticalSection(&_cs);
    if (hp._target != 0 && hp._call_site != 0) {
        BYTE* bptr = (BYTE*)hp._call_site;
        WORD* wptr = (WORD*)hp._call_site;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        
		map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(hp._call_site);
		if (it != _hooks.end()) {
			//already hooked something at this call site
			add_hook_point(0, hp);
			
		} else if (VirtualProtect(bptr, 8, newProtection, &protection)) {
			DWORD original = 0xffffffff;
			BYTE addrShift=1;
			BYTE commandLen=5;
			
            // get the type
            switch (bptr[0]) {
            case 0xe8:	//call
            	_typeMap[hp._call_site]=HOOKTYPE_CALL;
            	break;
            case 0xe9:	//jump
            	_typeMap[hp._call_site]=HOOKTYPE_JMP;
            	break;
            
            default:	//other command
				original=0;
				commandLen=5;
				_typeMap[hp._call_site]=HOOKTYPE_NONE;
            	break;
            };
            // get original target
            DWORD* ptr = (DWORD*)(hp._call_site + addrShift);
			if (original==0xffffffff)
				original = (DWORD)(ptr[0] + hp._call_site + 5);
            
            // keep track of this hp
            add_hook_point(original, hp);

            // hook by setting new target
            memset(bptr,0x90,commandLen);
            bptr[0] = 0xe8;
            ptr = (DWORD*)(hp._call_site + 1);
            ptr[0] = _callHandler - (DWORD)(hp._call_site + 5);
            result = true;

        } else {
            char buf[20];
            sprintf(buf,"%08x",hp._call_site);
            _last_error = "unable to change memory protection at " + string(buf);
        }
    } else {
        _last_error = "invalid hook_point: either _target or _call_site is null.";
    }
    LeaveCriticalSection(&_cs);
    return result;
}

bool hook_manager::unhook(hook_point& hp)
{
    bool result = false;
    EnterCriticalSection(&_cs);
    if (hp._target != 0 && hp._call_site != 0) {
        WORD* wptr = (WORD*)hp._call_site;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(wptr, 8, newProtection, &protection)) {
			BYTE addrShift=1;
			WORD commandBytes=0;
			
			switch (getType(hp._call_site)) {
			case HOOKTYPE_CALL:
				commandBytes=0xe8;
				break;
			case HOOKTYPE_JMP:
				commandBytes=0xe9;
				break;
			};
			
			wptr[0]=commandBytes;
            DWORD* ptr = (DWORD*)(hp._call_site + addrShift);

            // get new target
            DWORD new_target = remove_hook_point(hp);
            //not sure if this should be done...
            if (new_target != 0) {
                // set new target
                ptr[0] = new_target - (DWORD)(hp._call_site + 5);
            }
            result = true;

        } else {
            char buf[20];
            sprintf(buf,"%08x",hp._call_site);
            _last_error = "unable to change memory protection at " + string(buf);
        }
    } else {
        _last_error = "invalid hook_point: either _target or _call_site is null.";
    }
    LeaveCriticalSection(&_cs);
    return result;
}

// adds element to call-chain
void hook_manager::add_hook_point(DWORD original, hook_point& hp)
{
    map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(hp._call_site);
    if (it == _hooks.end()) {
        //first entry
        vector<DWORD>* v = new vector<DWORD>();
        if (original != 0)
        	v->push_back(original);
        v->push_back(hp._target);
        _hooks[hp._call_site] = v;
        _numArgsMap[hp._call_site] = hp._numArgs;
    } else {
        it->second->push_back(hp._target);
    }
}

// removes the element from call-chain
// returns new target
DWORD hook_manager::remove_hook_point(hook_point& hp)
{
    DWORD result = 0;
    map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(hp._call_site);
    if (it != _hooks.end()) {
        vector<DWORD>* v = it->second;
        vector<DWORD>::iterator vit = find(v->begin(), v->end(), hp._target);
        if (vit != v->end()) {
            v->erase(vit);
        }
        if (vit == v->begin())
        	result = v->back();
        else
        	result=0;
    }
    return result;
}

DWORD hook_manager::getFirstTarget(DWORD call_site, bool* lastAddress)
{
	DWORD result=0;
	EnterCriticalSection(&_cs);
	
	map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(call_site);
	if (it != _hooks.end()) {
		vector<DWORD>* v = it->second;
		vector<DWORD>::iterator vit = v->end();
		do {
			if (vit == v->begin()) {
				_last_error = "Couldn't find any valid target!";
				return 0;
			};
			vit--;
		} while (*vit==0);	
		_currCallPos[call_site]=vit;
		result=*vit;
		if (lastAddress!=NULL) {
			if (vit == v->begin())
				*lastAddress=true;
			else
				*lastAddress=false;
		};
		
		if (getType(call_site)==HOOKTYPE_CALLPTR && vit==v->begin()) {
			if (IsBadReadPtr((BYTE*)result,4)) {
				_last_error = "Invalid pointer to call address!";
				return 0;
			};
			result=*(DWORD*)result;
		};	
			
	} else {
		_last_error = "Nothing has been hooked at this call site!";
		return 0;
	};
	
	LeaveCriticalSection(&_cs);
	return result;
};

DWORD hook_manager::getNextTarget(DWORD call_site, bool* lastAddress)
{
	DWORD result=0;
	EnterCriticalSection(&_cs);
	
	map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(call_site);
	if (it != _hooks.end()) {
		vector<DWORD>* v = it->second;
		vector<DWORD>::iterator vit = _currCallPos[call_site];
		do {
			if (vit == v->begin()) {
				_last_error = "Couldn't find any valid target!";
				return 0;
			};
			vit--;
		} while (*vit==0);
		_currCallPos[call_site]=vit;
		result=*vit;
		if (lastAddress!=NULL) {
			if (vit == v->begin())
				*lastAddress=true;
			else
				*lastAddress=false;
		};

		if (getType(call_site)==HOOKTYPE_CALLPTR && vit==v->begin()) {
			if (IsBadReadPtr((BYTE*)result,4)) {
				_last_error = "Invalid pointer to call address!";
				return 0;
			};
			result=*(DWORD*)result;
		};	

	} else {
		_last_error = "Nothing has been hooked at this call site!";
		return 0;
	};

	LeaveCriticalSection(&_cs);
	return result;
};

DWORD hook_manager::getOriginalTarget(DWORD call_site)
{
	DWORD result=0;
	EnterCriticalSection(&_cs);
	
	map<DWORD,vector<DWORD>*>::iterator it = _hooks.find(call_site);
	if (it != _hooks.end()) {
		vector<DWORD>* v = it->second;
		vector<DWORD>::iterator vit = v->begin();
		if (vit==v->end()) {
			_last_error = "Couldn't find the original target!";
			return 0;
		};
		
		result=*vit;
			
	} else {
		_last_error = "Nothing has been hooked at this call site!";
		return 0;
	};
	
	LeaveCriticalSection(&_cs);
	return result;
};

DWORD hook_manager::getNumArgs(DWORD call_site)
{
	DWORD result=0;
	EnterCriticalSection(&_cs);
	
	map<DWORD,DWORD>::iterator it = _numArgsMap.find(call_site);
	if (it != _numArgsMap.end()) {
		result=it->second;
	};

	LeaveCriticalSection(&_cs);
	return result;
};

DWORD hook_manager::getType(DWORD call_site)
{
	DWORD result=HOOKTYPE_NONE;
	EnterCriticalSection(&_cs);
	
	map<DWORD,DWORD>::iterator it = _typeMap.find(call_site);
	if (it != _typeMap.end()) {
		result=it->second;
	};

	LeaveCriticalSection(&_cs);
	return result;
};

string hook_manager::get_last_error()
{
    return _last_error;
}

hook_manager::~hook_manager()
{
    EnterCriticalSection(&_cs);
    for (map<DWORD,vector<DWORD>*>::iterator it = _hooks.begin(); it != _hooks.end(); it++) {
        vector<DWORD>* v = it->second;
        delete v;
    }
    _hooks.erase(_hooks.begin(), _hooks.end());
    _currCallPos.erase(_currCallPos.begin(), _currCallPos.end());
    _numArgsMap.erase(_numArgsMap.begin(), _numArgsMap.end());
    _typeMap.erase(_typeMap.begin(), _typeMap.end());
    LeaveCriticalSection(&_cs);
    DeleteCriticalSection(&_cs);
}


