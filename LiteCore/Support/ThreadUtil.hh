//
// ThreadUtil.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#ifndef _MSC_VER
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#else
#include <Windows.h>
#include <atlbase.h>
#endif

#include <thread>
#include <string>
#include <sstream>

namespace litecore {

#ifdef _MSC_VER
    // This is sickening, but it is the only way to set thread names in MSVC.
    // By raising an SEH exception (Windows equivalent to Unix signal)
    // then catching and ignoring it
    // https://stackoverflow.com/a/10364541/1155387

    const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
    typedef struct
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    typedef HRESULT(*SetThreadNameCall)(HANDLE, PCWSTR);
    typedef HRESULT(*GetThreadNameCall)(HANDLE, PWSTR*);
    static HINSTANCE kernelLib = LoadLibrary(TEXT("kernel32.dll"));

    static bool TryNewSetThreadName(const char* name) {
        // According to docs, on some systems this function is only available this way
        
        static bool valid = false;
        if(kernelLib != NULL) {
            static SetThreadNameCall setThreadNameCall = (SetThreadNameCall)GetProcAddress(kernelLib, "SetThreadDescription");
            if(setThreadNameCall != NULL) {
                CA2WEX<256> wide(name, CP_UTF8);
                setThreadNameCall(GetCurrentThread(), wide);
                valid = true;
            }
        }

        return valid;
    }
#endif

    static inline void SetThreadName(const char *name) {
#ifndef _MSC_VER
        {
#ifdef __APPLE__
            pthread_setname_np(name);
#else
            pthread_setname_np(pthread_self(), name);
#endif
        }
#else

        if(!TryNewSetThreadName(name)) {
            return;
        }

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = -1;
        info.dwFlags = 0;

        __try {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        } __except(EXCEPTION_EXECUTE_HANDLER) {

        }
#endif
    }

    static inline std::string GetThreadName() {
        std::string retVal;
        std::stringstream s;
#ifndef _MSC_VER
        char name[256];
        if(pthread_getname_np(pthread_self(), name, 255) == 0 && name[0] != 0) {
            s << name << " ";
        }

        pid_t tid;
#ifdef __APPLE__
        // FreeBSD only pthread call, cannot use with glibc, and conversely syscall
        // is deprecated in macOS 10.12+
        uint64_t tmp;
        pthread_threadid_np(pthread_self(), &tmp);
        tid = (pid_t)tmp;
#else
        tid = syscall(__NR_gettid);
#endif
        
        s << "(" << tid<< ")";
        retVal = s.str();
#else
        if(kernelLib != NULL) {
            static GetThreadNameCall getThreadNameCall = (GetThreadNameCall)GetProcAddress(kernelLib, "GetThreadDescription");
            if(getThreadNameCall != NULL) {
                wchar_t *buf;
                HRESULT r = getThreadNameCall(GetCurrentThread(), &buf);
                if(SUCCEEDED(r)) {
                    CW2AEX<256> mb(buf, CP_UTF8);
                    retVal = mb;
                    LocalFree(buf);
                }
            }
        }
#endif

        if(retVal.size() == 0) {
            s << std::this_thread::get_id();
            retVal = s.str();
        }

        return retVal;
    }
}
