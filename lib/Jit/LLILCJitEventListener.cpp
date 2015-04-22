//===---- lib/Jit/LLILCJitEventListener.cpp ---------------------*- C++ -*-===//
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
/// \brief Define a JITEventListener object to tell CLR EE about JITted
/// functions.
///
//===----------------------------------------------------------------------===//

#include "llvm/Config/config.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/raw_ostream.h"
#include "jitpch.h"
#include "LLILCJit.h"
#include "LLILCJitEventListener.h"
#include <string>

using namespace llvm;
using namespace llvm::object;

void LLILCJitEventListener::NotifyObjectEmitted(
    const ObjectFile &Obj, const RuntimeDyld::LoadedObjectInfo &L) {

  OwningBinary<ObjectFile> DebugObjOwner = L.getObjectForDebug(Obj);
  const ObjectFile &DebugObj = *DebugObjOwner.getBinary();

  // Get the address of the object image for use as a unique identifier
  const void *ObjData = DebugObj.getData().data();
  DIContext *Context = new DWARFContextInMemory(DebugObj);
  MethodAddressVector Functions;

  // Use symbol info to iterate functions in the object.

  for (symbol_iterator I = DebugObj.symbol_begin(), E = DebugObj.symbol_end();
       I != E; ++I) {
    SymbolRef::Type SymType;
    if (I->getType(SymType))
      continue;
    if (SymType == SymbolRef::ST_Function) {
      // Function info
      StringRef Name;
      uint64_t Addr;
      uint64_t Size;
      if (I->getName(Name))
        continue;
      if (I->getAddress(Addr))
        continue;
      if (I->getSize(Size))
        continue;

      Functions.push_back((void *)Addr);

      unsigned LastDebugOffset = -1;
      unsigned CurrentDebugEntry = 0;
      unsigned NumDebugRanges = 0;
      unsigned Offset = 0;
      ICorDebugInfo::OffsetMapping *OM;
      // unsigned LastDebugEntry = 0;

      if (Context) {
        DILineInfoTable Lines = Context->getLineInfoForAddressRange(Addr, Size);

        DILineInfoTable::iterator Begin = Lines.begin();
        DILineInfoTable::iterator End = Lines.end();

        // Count offset entries. Will skip an entry if the current IL offset matches
        // the previous offset.
        for (DILineInfoTable::iterator It = Begin; It != End; ++It) {
          int LineNumber = (It->second).Line;

          if (LineNumber != LastDebugOffset) {
            NumDebugRanges++;
            LastDebugOffset = LineNumber;
          }
        }

        // Reset offset
        LastDebugOffset = -1;

        if (NumDebugRanges > 0) {

          // Allocate OffsetMapping array
          unsigned SizeOfArray =
              (NumDebugRanges) * sizeof(ICorDebugInfo::OffsetMapping);
          OM = (ICorDebugInfo::OffsetMapping *)
                   Wrapper->Context->JitInfo->allocateArray(SizeOfArray);

          Begin = Lines.begin();

          // Iterate through the debug entries and save IL offset, native offset, and
          // source reason
          for (DILineInfoTable::iterator It = Begin; It != End; ++It) {
            int Offset = It->first;
            int LineNumber = (It->second).Line;

            // We store info about if the instruction is being recorded because
            // it is a call in the column field
            int IsCall = (It->second).Column;

            if (LineNumber != LastDebugOffset) {
              LastDebugOffset = LineNumber;
              OM[CurrentDebugEntry].nativeOffset = Offset;
              OM[CurrentDebugEntry].ilOffset = LineNumber;
              OM[CurrentDebugEntry].source =
                  IsCall == 1 ? ICorDebugInfo::CALL_INSTRUCTION
                              : ICorDebugInfo::STACK_EMPTY;
              CurrentDebugEntry++;
            }
          }

          // Send array of OffsetMappings to CLR EE
          Wrapper->NotifyEvent(OM, NumDebugRanges);
        }
      }
    }
  }

  // To support object unload notification, we need to keep a list of
  // registered function addresses for each loaded object.  We will
  // use the MethodIDs map to get the registered ID for each function.
  LoadedObjectMap[ObjData] = Functions;
  DebugObjects[Obj.getData().data()] = std::move(DebugObjOwner);
}

void LLILCJitEventListener::NotifyFreeingObject(const ObjectFile &Obj) {
  // This object may not have been registered with the listener. If it wasn't,
  // bail out.
  if (DebugObjects.find(Obj.getData().data()) == DebugObjects.end())
    return;

  // Get the address of the object image for use as a unique identifier
  const ObjectFile &DebugObj = *DebugObjects[Obj.getData().data()].getBinary();
  const void *ObjData = DebugObj.getData().data();

  // Get the object's function list from LoadedObjectMap
  ObjectMap::iterator OI = LoadedObjectMap.find(ObjData);
  if (OI == LoadedObjectMap.end())
    return;
  MethodAddressVector &Functions = OI->second;

  // Walk the function list, unregistering each function
  for (MethodAddressVector::iterator FI = Functions.begin(),
                                     FE = Functions.end();
       FI != FE; ++FI) {
    void *FnStart = const_cast<void *>(*FI);
    MethodIDMap::iterator MI = MethodIDs.find(FnStart);
    if (MI != MethodIDs.end()) {
      MethodIDs.erase(MI);
    }
  }

  // Erase the object from LoadedObjectMap
  LoadedObjectMap.erase(OI);
  DebugObjects.erase(Obj.getData().data());
}

JITEventListener *LLILCJitEventListener::createLLILCJitEventListener() {
  return new LLILCJitEventListener(new LLILCJitEventWrapper);
}

JITEventListener *LLILCJitEventListener::createLLILCJitEventListener(
    LLILCJitEventWrapper *TestImpl) {
  return new LLILCJitEventListener(TestImpl);
}
