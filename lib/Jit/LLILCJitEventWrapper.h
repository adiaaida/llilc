//===---- lib/Jit/LLILCJitEventWrapper.h ------------------------*- C++ -*-===//
//
// LLILC
//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.
// See LICENSE file in the project root for full license information.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Define a JitEventWrapper object to tell CLR EE about JITted
/// functions.
///
//===----------------------------------------------------------------------===//

#include "llvm/Support/DataTypes.h"
#include "jitpch.h"
#include "LLILCJit.h"

namespace llvm {

class LLILCJitEventWrapper {

public:
  LLILCJitContext *Context;

  LLILCJitEventWrapper() : Context(nullptr) {}

  LLILCJitEventWrapper(LLILCJitContext *JitContext) : Context(JitContext) {}

  void NotifyEvent(ICorDebugInfo::OffsetMapping *OM, unsigned NumDebugRanges) {
    CORINFO_METHOD_INFO *MethodInfo = Context->MethodInfo;
    CORINFO_METHOD_HANDLE MethodHandle = MethodInfo->ftn;

    Context->JitInfo->setBoundaries(MethodHandle, NumDebugRanges, OM);
  }

private:
};

} // namespace llvm
