#pragma once

#include "common/types.h"
#include <windows.h>
#include <string>

namespace Common {

class CrashHandler {
public:
    static void Install();
    static LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
};

} // namespace Common
