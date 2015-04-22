#include "llvm/ADT/DenseMap.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/Object/ObjectFile.h"
#include "LLILCJitEventWrapper.h"
#include <string>

using namespace llvm;
using namespace llvm::object;

class LLILCJitEventListener : public JITEventListener {
  typedef DenseMap<void *, unsigned int> MethodIDMap;

  MethodIDMap MethodIDs;

  typedef SmallVector<const void *, 64> MethodAddressVector;
  typedef DenseMap<const void *, MethodAddressVector> ObjectMap;

  ObjectMap LoadedObjectMap;
  std::map<const char *, OwningBinary<ObjectFile>> DebugObjects;
  std::unique_ptr<LLILCJitEventWrapper> Wrapper;

public:
  LLILCJitEventListener(LLILCJitEventWrapper *LibraryWrapper) {
    Wrapper.reset(LibraryWrapper);
  }

  ~LLILCJitEventListener() {}

  void NotifyObjectEmitted(const ObjectFile &Obj,
                           const RuntimeDyld::LoadedObjectInfo &L) override;

  void NotifyFreeingObject(const ObjectFile &Obj) override;

  // Construct a LLILCJitEventListener
  static JITEventListener *createLLILCJitEventListener();

  // Construct a LLILCJitEventListener with a test LLILC Jit API implementation
  static JITEventListener *
  createLLILCJitEventListener(LLILCJitEventWrapper *AlternativeImpl);
};
