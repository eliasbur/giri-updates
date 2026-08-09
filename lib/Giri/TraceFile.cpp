//===- Giri.h - Dynamic Slicing Pass ----------------------------*- C++ -*-===//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files implements the classes for reading the trace file.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giri"

#include "Giri/TraceFile.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

#include <cassert>
#include <vector>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace giri;
using namespace llvm;
using namespace std;

STATISTIC(NumStaticBuggyVal, "Num. of possible missing matched static values");
STATISTIC(NumDynBuggyVal, "Number of possible missing matched dynamic values");

TraceFile::TraceFile(string Filename,
                      const QueryBasicBlockNumbers *bbNums,
                      const QueryLoadStoreNumbers *lsNums) :
  bbNumPass(bbNums), lsNumPass(lsNums),
  trace(0), totalLoadsTraced(0), lostLoadsTraced(0) {
  int fd = open(Filename.c_str(), O_RDONLY);
  assert((fd > 0) && "Cannot open file!\n");

  struct stat finfo;
  int ret = fstat(fd, &finfo);
  assert((ret == 0) && "Cannot fstat() file!\n");
  maxIndex = finfo.st_size / sizeof(Entry) - 1;

  trace = (Entry *)mmap(0,
                        finfo.st_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE,
                        fd,
                        0);
  assert((trace != MAP_FAILED) && "Trace mmap() failed!\n");

  fixupLostLoads();
  buildTraceFunAddrMap();

  DEBUG(dbgs() << "TraceFile " << Filename << " successfully initialized.\n");
}

DynValue *TraceFile::getLastDynValue(Value  *V) {
  Instruction *I = dyn_cast<Instruction>(V);
  if (I == nullptr)
    return new DynValue(V, 0);

  unsigned id = bbNumPass->getID(I->getParent());
  assert(id && "Basic block does not have ID!\n");

  for (unsigned long index = maxIndex; index > 0; --index) {
    if (trace[index].type == RecordType::BBType && trace[index].id == id)
      return new DynValue(I, index);
  }

  assert(trace[0].type == RecordType::BBType && trace[0].id == id &&
         "Cannot find instruction in trace!\n");

  return new DynValue(I, 0);
}

void TraceFile::getSourcesFor(DynValue &DInst, Worklist_t &Worklist) {
  if (BranchInst *BI = dyn_cast<BranchInst>(DInst.V)) {
    if (BI->isConditional()) {
      DynValue NDV = DynValue(BI->getCondition(), DInst.index);
      addToWorklist(NDV, Worklist, DInst);
    }
  } else if (SwitchInst *SI = dyn_cast<SwitchInst>(DInst.V)) {
    DynValue NDV = DynValue(SI->getCondition(), DInst.index);
    addToWorklist(NDV, Worklist, DInst);
  } else if (isa<PHINode>(DInst.V)) {
    getSourcesForPHI(DInst, Worklist);
  } else if (isa<SelectInst>(DInst.V)) {
    getSourceForSelect(DInst, Worklist);
  } else if (isa<Argument>(DInst.V)) {
    getSourcesForArg(DInst, Worklist);
  } else if (LoadInst *LI = dyn_cast<LoadInst>(DInst.V)) {
    DynValue NDV = DynValue(LI->getOperand(0), DInst.index);
    addToWorklist(NDV, Worklist, DInst);
    getSourcesForLoad(DInst, Worklist);
  } else if (isa<CallInst>(DInst.V)) {
    if (!getSourcesForSpecialCall(DInst, Worklist))
      getSourcesForCall(DInst, Worklist);
  } else if (Instruction *I = dyn_cast<Instruction>(DInst.V)) {
    for (unsigned index = 0; index < I->getNumOperands(); ++index)
      if (!isa<Constant>(I->getOperand(index))) {
        DynValue NDV = DynValue(I->getOperand(index), DInst.index);
        addToWorklist(NDV, Worklist, DInst);
      }
  }
}

DynBasicBlock TraceFile::getExecForcer(DynBasicBlock DBB,
                                        const set<unsigned> &bbnums) {
  if (!normalize(DBB))
    return DynBasicBlock(nullptr, maxIndex);

  unsigned long index = findPreviousID(DBB.BB->getParent(),
                                        DBB.index - 1,
                                        RecordType::BBType,
                                        trace[DBB.index].tid,
                                        bbnums);

  if (index == maxIndex)
    return DynBasicBlock(nullptr, maxIndex);

  assert(trace[index].type == RecordType::BBType);
  return DynBasicBlock(bbNumPass->getBlock(trace[index].id), index);
}

void TraceFile::addToWorklist(DynValue &DV,
                               Worklist_t &Sources,
                               DynValue &Parent) {
  DynValue *temp = new DynValue(DV);
  temp->setParent(&Parent);
  Sources.push_front(temp);
}

bool TraceFile::normalize(DynBasicBlock &DBB) {
  if (BuggyValues.find(DBB.BB) != BuggyValues.end()) {
    NumDynBuggyVal++;
    return false;
  }

  unsigned bbID = bbNumPass->getID(DBB.BB);
  unsigned long index = findPreviousID(DBB.BB->getParent(),
                                        DBB.index,
                                        RecordType::BBType,
                                        trace[DBB.index].tid,
                                        bbID);
  if (index == maxIndex) {
    DEBUG(errs() << "Buggy values found at normalization. Function name: "
                  << DBB.BB->getParent()->getName().str() << "\n");
    BuggyValues.insert(DBB.BB);
    NumStaticBuggyVal++;
    return false;
  }

  assert(trace[index].type == RecordType::ENType ||
         (trace[index].type == RecordType::BBType && trace[index].id == bbID));

  DBB.index = index;
  return true;
}

bool TraceFile::normalize(DynValue &DV) {
  if (BuggyValues.find(DV.V) != BuggyValues.end()) {
    NumDynBuggyVal++;
    return false;
  }

  BasicBlock *BB = nullptr;
  if (Instruction *I = dyn_cast<Instruction>(DV.V))
    BB = I->getParent();
  else if (Argument *Arg = dyn_cast<Argument>(DV.V))
    BB = &(Arg->getParent()->getEntryBlock());

  if (!BB)
    return true;

  Function *fun = BB->getParent();
  unsigned bbID = bbNumPass->getID(BB);
  unsigned long normIndex = findPreviousID(fun,
                                            DV.index,
                                            RecordType::BBType,
                                            trace[DV.index].tid,
                                            bbID);
  if (normIndex == maxIndex) {
    DEBUG(errs() << "Buggy values found at normalization. Function name: "
                  << fun->getName().str() << "\n");
    BuggyValues.insert(DV.V);
    NumStaticBuggyVal++;
    return false;
  }

  assert(trace[normIndex].type == RecordType::BBType);

  DV.index = normIndex;
  return true;
}

struct EntryCompare {
  bool operator()(const Entry &e1, const Entry &e2) const {
    return e1.address < e2.address && (e1.address + e1.length - 1 < e2.address);
  }
};

void TraceFile::fixupLostLoads() {
  set<Entry, EntryCompare> Stores;

  for (unsigned long index = 0;
       trace[index].type != RecordType::ENType;
       ++index)
    switch (trace[index].type) {
      case RecordType::STType: {
        Entry newEntry = trace[index];
        set<Entry>::iterator st;
        while ((st = Stores.find(newEntry)) != Stores.end()) {
          uintptr_t address = (st->address < newEntry.address) ?
                               st->address : newEntry.address;
          uintptr_t endst   =  st->address + st->length - 1;
          uintptr_t end     =  newEntry.address + newEntry.length - 1;
          uintptr_t maxend  = (endst < end) ? end : endst;
          uintptr_t length  = maxend - address + 1;
          newEntry.address = address;
          newEntry.length = length;
          Stores.erase(st);
        }
        Stores.insert(newEntry);
        break;
      }
      case RecordType::LDType: {
        if (Stores.find(trace[index]) == Stores.end()) {
          DEBUG(dbgs() << "Fixing load for index " << index << "\n");
          trace[index].address = 0;
        }
        break;
      }
      default:
        break;
    }
}

void TraceFile::buildTraceFunAddrMap(void) {
  for (unsigned long index = 0;
       trace[index].type != RecordType::ENType;
       ++index) {
    if (trace[index].type == RecordType::CLType) {
      Instruction *V = lsNumPass->getInstByID(trace[index].id);
      if (CallInst *CI = dyn_cast<CallInst>(V))
        if (Function *calledFun = CI->getCalledFunction())
          if (traceFunAddrMap.find(calledFun) == traceFunAddrMap.end())
            traceFunAddrMap[calledFun] = trace[index].address;
    }
  }

  DEBUG(dbgs() << "traceFunAddrMap.size(): " << traceFunAddrMap.size() << "\n");
}

unsigned long TraceFile::findPreviousID(unsigned long start_index,
                                         RecordType type,
                                         pthread_t tid,
                                         const unsigned id) {
  unsigned long index = start_index;
  while (true) {
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].id == id)
      return index;
    if (index == 0)
      break;
    --index;
  }

  if (type == RecordType::BBType) {
    for (index = maxIndex; trace[index].type != RecordType::ENType; --index)
      ;
    return index;
  }

  report_fatal_error("Did not find desired trace entry!");
}

unsigned long TraceFile::findPreviousID(Function *fun,
                                         unsigned long start_index,
                                         RecordType type,
                                         pthread_t tid,
                                         const set<unsigned> &ids) {
  uintptr_t funAddr;
  if (traceFunAddrMap.find(fun) != traceFunAddrMap.end())
     funAddr = traceFunAddrMap[fun];
  else
     funAddr = ~0;

  unsigned long index = start_index;
  signed nesting = 0;
  do {
    assert(nesting >= 0);

    if (trace[index].type == type &&
        trace[index].tid == tid &&
        ids.count(trace[index].id)) {
      if (nesting == 0)
        return index;
      else if (nesting == 1 &&
               type == RecordType::CLType &&
               trace[index].type == RecordType::CLType &&
               trace[index].tid == tid &&
               trace[index].address == funAddr)
        return index;
    }

    if (trace[index].type == RecordType::RTType &&
        trace[index].tid == tid &&
        trace[index].address == funAddr)
      ++nesting;

    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == tid &&
        trace[index].address == funAddr)
      --nesting;

    --index;
  } while (index != 0);

  return maxIndex;
}

unsigned long TraceFile::findPreviousID(Function *fun,
                                         unsigned long start_index,
                                         RecordType type,
                                         pthread_t tid,
                                         const unsigned id) {
  set<unsigned> ids;
  ids.insert(id);
  return findPreviousID(fun, start_index, type, tid, ids);
}

unsigned long TraceFile::findPreviousNestedID(unsigned long start_index,
                                               RecordType type,
                                               pthread_t tid,
                                               const unsigned id,
                                               const unsigned nestedID) {
  assert(trace[start_index].type == RecordType::BBType);
  assert(type != RecordType::BBType);
  assert(start_index > 0);

  unsigned long index = start_index;
  unsigned nesting = 0;
  do {
    --index;
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].id == id) {
      if (nesting == 0) {
        return index;
      } else {
        --nesting;
        continue;
      }
    }

    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == tid &&
        trace[index].id == nestedID)
      ++nesting;
  } while (index != 0);

  report_fatal_error("No proper basic block at the nesting level");
}

unsigned long TraceFile::findNextNestedID(unsigned long start_index,
                                           RecordType type,
                                           const unsigned id,
                                           const unsigned nestID,
                                           pthread_t tid) {
  unsigned nesting = 0;
  unsigned long index = start_index;
  while (true) {
    if (index > maxIndex)
      break;

    if (trace[index].type == type &&
        trace[index].id == id &&
        trace[index].tid == tid) {
      if (nesting == 0)
        return index;
      else
        --nesting;
    }

    if (trace[index].type == RecordType::BBType &&
        trace[index].id == nestID &&
        trace[index].tid == tid)
      ++nesting;

    ++index;
  }

  errs() << "start_index: " << start_index
         << " type: " << static_cast<char>(type)
         << " id: " << id
         << " nestID: " << nestID << "\n";
  report_fatal_error("Did not find desired subsequent entry in trace!");
}

unsigned long TraceFile::findNextAddress(unsigned long start_index,
                                          RecordType type,
                                          pthread_t tid,
                                          const uintptr_t address) {
  unsigned long index = start_index;
  while (index <= maxIndex) {
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].address == address)
      return index;
    ++index;
  }

  errs() << "start_index: " << start_index
         << " type: " << static_cast<char>(type)
         << " tid: " << tid
         << " address: " << address << "\n";
  report_fatal_error("Did not find desired subsequent entry in trace!");
}

void TraceFile::getSourcesForPHI(DynValue &DV, Worklist_t &Sources) {
  PHINode *PHI = dyn_cast<PHINode>(DV.V);
  assert(PHI && "Caller passed us a non-PHI value!\n");

  Function *Func = PHI->getParent()->getParent();
  unsigned phiID = bbNumPass->getID(PHI->getParent());
  unsigned long block_index = findPreviousID(Func,
                                              DV.index,
                                              RecordType::BBType,
                                              trace[DV.index].tid,
                                              phiID);
  if (block_index == maxIndex) {
    errs() << __func__ << " failed DV.\n";
    return;
  }
  assert(block_index > 0);

  set<unsigned> predIDs;
  for (unsigned index = 0; index < PHI->getNumIncomingValues(); ++index)
    predIDs.insert(bbNumPass->getID(PHI->getIncomingBlock(index)));
  unsigned long pred_index = findPreviousID(Func,
                                             block_index - 1,
                                             RecordType::BBType,
                                             trace[DV.index].tid,
                                             predIDs);
  if (pred_index == maxIndex) {
    errs() << __func__ << " failed BLOCK.\n";
    return;
  }

  unsigned predBBID = trace[pred_index].id;
  for (unsigned index = 0; index < PHI->getNumIncomingValues(); ++index)
    if (predBBID == bbNumPass->getID(PHI->getIncomingBlock(index))) {
      Value *V = PHI->getIncomingValue(index);
      DynValue NDV = DynValue(V, pred_index);
      addToWorklist(NDV, Sources, DV);
      return;
    }

  report_fatal_error("No predecessor BB found for PHI!");
}

void TraceFile::getSourcesForArg(DynValue &DV, Worklist_t &Sources) {
  Argument *Arg = dyn_cast<Argument>(DV.V);
  assert(Arg && "Caller passed a non-argument dynamic instance!\n");

  if (Arg->getParent()->getName().str() == "main")
    return;

  if (!normalize(DV))
    return;

  assert(DV.index > 0);
  unsigned long callIndex = DV.index - 1;
  while (callIndex > 0) {
    if (trace[callIndex].type == RecordType::CLType &&
        trace[callIndex].tid == trace[DV.index].tid &&
        trace[callIndex].address == trace[DV.index].address)
      break;
    --callIndex;
  }
  assert(callIndex < DV.index);

  if (trace[callIndex].type != RecordType::CLType ||
      trace[callIndex].tid != trace[DV.index].tid ||
      trace[callIndex].address != trace[DV.index].address) {
    errs() << "For some variable length functions like ap_rprintf in apache, "
              "call records missing. Stop here for now. Fix it later\n";
    DV.getValue()->print(errs()); errs() << "\n";
    return;
  }

  unsigned callid = trace[callIndex].id;
  CallInst *CI = dyn_cast<CallInst>(lsNumPass->getInstByID(callid));
  assert(CI);

  bool found = false;
  unsigned nesting = 0;
  unsigned long index = callIndex;
  unsigned bbid = bbNumPass->getID(CI->getParent());
  while (!found) {
    assert(index <= maxIndex);

    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == trace[callIndex].tid &&
        trace[index].id == trace[callIndex].id) {
      ++nesting;
      ++index;
      continue;
    }

    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == trace[callIndex].tid &&
        trace[index].id == bbid) {
      if (--nesting == 0)
        break;
    }

    if (trace[index].type == RecordType::ENType)
      break;

    ++index;
  }

  assert(index <= maxIndex);

  Function *CalledFunc = CI->getCalledFunction();
  if (CalledFunc && CalledFunc->isDeclaration()) {
    if (CalledFunc->getName().str() == "pthread_create") {
      for (uint i=0; i<CI->getNumOperands()-1; i++)
        if (!isa<Constant>(CI->getOperand(i))) {
          DynValue NDV = DynValue(CI->getOperand(i), index);
          addToWorklist(NDV, Sources, DV);
        }
      return;
    }
  } else {
    DynValue NDV = DynValue(CI->getOperand(Arg->getArgNo()), index);
    addToWorklist(NDV, Sources, DV);
    return;
  }
}

static inline bool overlaps(const Entry &first, const Entry &second) {
  if (first.address < second.address &&
      first.address + first.length - 1 < second.address)
    return false;

  if (second.address < first.address &&
      second.address + second.length - 1 < first.address)
    return false;

  return true;
}

void TraceFile::findAllStoresForLoad(DynValue &DV,
                                      Worklist_t &Sources,
                                      long store_index,
                                      const Entry load_entry) {
  while (store_index >= 0) {
    if (trace[store_index].type == RecordType::STType &&
        overlaps(trace[store_index], load_entry)) {
      Instruction *SI = lsNumPass->getInstByID(trace[store_index].id);
      assert(SI);

      unsigned storeBBID = bbNumPass->getID(SI->getParent());
      unsigned long bbindex = findNextNestedID(store_index,
                                                RecordType::BBType,
                                                storeBBID,
                                                trace[store_index].id,
                                                trace[store_index].tid);
      DynValue NDV = DynValue(SI, bbindex);
      addToWorklist(NDV, Sources, DV);

      Entry &store_entry = trace[store_index];
      if (load_entry.address < store_entry.address) {
        Entry new_entry;
        new_entry.address = load_entry.address;
        new_entry.length = store_entry.address - load_entry.address;
        findAllStoresForLoad(DV, Sources, store_index - 1, new_entry);
      }

      unsigned long store_end = store_entry.address + store_entry.length;
      unsigned long load_end = load_entry.address + load_entry.length;
      if (store_end < load_end) {
        Entry new_entry;
        new_entry.address = store_end;
        new_entry.length = load_end - store_end;
        findAllStoresForLoad(DV, Sources, store_index - 1, new_entry);
      }
      break;
    }
    --store_index;
  }

  if (store_index == -1) {
    DEBUG(dbgs() << "We can't find the source of the load:");
    DEBUG(DV.getValue()->print(dbgs()));
    Instruction *LI = dyn_cast<Instruction>(DV.getValue());
    DEBUG(dbgs() << " ID: " << lsNumPass->getID(LI));
    DEBUG(dbgs() << "\n");
    ++lostLoadsTraced;
  }
}

void TraceFile::getSourcesForLoad(DynValue &DV,
                                   Worklist_t &Sources,
                                   unsigned count) {
  Instruction *I = dyn_cast<Instruction>(DV.V);
  assert(I && "Called with non-instruction dynamic instruction instance!\n");

  unsigned loadID = lsNumPass->getID(I);
  assert(loadID && "load does not have an ID!\n");
  unsigned bbID = bbNumPass->getID(I->getParent());

  if (count == 0)
     return;

  if (!normalize(DV))
    return;

  std::vector<unsigned long> load_indices(count);
  unsigned long start_index = findPreviousNestedID(DV.index,
                                                    RecordType::LDType,
                                                    trace[DV.index].tid,
                                                    loadID,
                                                    bbID);
  load_indices[0]= start_index;
  for (unsigned index = 1; index < count; ++index) {
    start_index = findPreviousID(start_index - 1,
                                  RecordType::LDType,
                                  trace[DV.index].tid,
                                  loadID);
    load_indices[index]= start_index;
  }

  for (unsigned index = 0; index < count; ++index) {
    ++totalLoadsTraced;
    long block_index = load_indices[index];

    if (!trace[block_index].address) {
      ++lostLoadsTraced;
      continue;
    }

    long store_index = block_index - 1;
    findAllStoresForLoad(DV, Sources, store_index, trace[block_index]);
  }

  return;
}

bool TraceFile::getSourcesForSpecialCall(DynValue &DV,
                                          Worklist_t &Sources) {
  Instruction *I = dyn_cast<Instruction>(DV.V);
  if (!(isa<CallInst>(I) || isa<InvokeInst>(I)))
    return false;

  if (isa<DbgInfoIntrinsic>(I))
    return true;

  const CallSite CS(I);
  Function *CalledFunc = CS.getCalledFunction();
  if (!CalledFunc)
    return false;

  LLVMContext &Context = I->getParent()->getParent()->getParent()->getContext();
  Type *Int8Type = IntegerType::getInt8Ty(Context);
  const Type *VoidPtrType = PointerType::getUnqual(Int8Type);

  unsigned trace_index = DV.index;

  const StringRef name = CalledFunc->getName();
  if (name.startswith("llvm.memset.") || name == "calloc") {
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    return true;
  } else if (name.startswith("llvm.memcpy.") ||
             name.startswith("llvm.memmove.") ||
             name == "strcpy" ||
             name == "strlen") {
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    getSourcesForLoad(DV, Sources);
    return true;
  } else if (name == "strcat") {
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    getSourcesForLoad(DV, Sources, 2);
    return true;
  } else if (name == "tolower" || name == "toupper") {
  } else if (name == "fscanf") {
  } else if (name == "sscanf") {
  } else if (name == "sprintf") {
    unsigned numCharArrays = 0;
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
        if (CS.getArgument(index)->getType() == VoidPtrType && index >= 2)
          ++numCharArrays;
      }
    getSourcesForLoad(DV, Sources, numCharArrays);
    return true;
  } else if (name == "fgets") {
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    return true;
  }

  return false;
}

unsigned long TraceFile::matchReturnWithCall(unsigned long start_index,
                                              const unsigned bbID,
                                              const unsigned callID,
                                              pthread_t tid) {
  assert(trace[start_index].type == RecordType::BBType);
  assert(start_index > 0);

  unsigned long index = start_index;
  unsigned nesting = 0;
  do {
    --index;

    if (trace[index].type == RecordType::RTType &&
        trace[index].id == callID &&
        trace[index].tid == tid)
      if (nesting == 0)
        return index;

    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == tid &&
        trace[index].id == callID) {
      if (nesting == 0)
        report_fatal_error("Could NOT find a matching return entry for call!");
      else {
        --nesting;
        continue;
      }
    }

    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == tid &&
        trace[index].id == bbID)
      ++nesting;
  } while (index != 0);

  report_fatal_error("Can't find matching call at the proper nesting level.");
}

void TraceFile::getSourcesForCall(DynValue &DV, Worklist_t &Sources) {
  CallInst *CI = dyn_cast<CallInst>(DV.V);
  assert(CI && "Caller passed us a non-call value!\n");

  if (!normalize(DV))
    return;

  Function *CalledFunc = CI->getCalledFunction();
  if (!CalledFunc) {
    unsigned callID = lsNumPass->getID(CI);
    Function *Func = CI->getParent()->getParent();
    unsigned long callIndex = findPreviousID(Func,
                                             DV.index,
                                             RecordType::CLType,
                                             trace[DV.index].tid,
                                             callID);
    if (callIndex == maxIndex) {
      errs() << __func__ << " failed to find\n";
      return;
    }

    uintptr_t fp = trace[callIndex].address;
    if (trace[callIndex + 1].type == RecordType::RTType &&
        trace[callIndex + 1].tid == trace[callIndex].tid &&
        trace[callIndex + 1].id == trace[callIndex].id &&
        trace[callIndex + 1].address == trace[callIndex].address) {
      errs() << "Most likely an (indirect) external call. Check to make sure\n";
      for (unsigned index = 0; index < CI->getNumOperands(); ++index)
        if (!isa<Constant>(CI->getOperand(index))) {
          DynValue NDV = DynValue(CI->getOperand(index), DV.index);
          addToWorklist(NDV, Sources, DV);
        }
      return;
    }

    unsigned long targetEntryBB = findNextAddress(callIndex + 1,
                                                  RecordType::BBType,
                                                  trace[callIndex].tid,
                                                  fp);
    if (targetEntryBB == maxIndex)
      return;

    BasicBlock *TargetEntryBB = bbNumPass->getBlock(trace[targetEntryBB].id);
    CalledFunc = TargetEntryBB->getParent();
  }
  assert(CalledFunc && "Could not find call function!\n");

  if (CalledFunc->isDeclaration()) {
    for (unsigned index = 0; index < CI->getNumOperands(); ++index)
      if (!isa<Constant>(CI->getOperand(index))) {
        DynValue NDV = DynValue(CI->getOperand(index), DV.index);
        addToWorklist(NDV, Sources, DV);
      }
    return;
  }

  unsigned bbID = bbNumPass->getID(CI->getParent());
  unsigned callID = lsNumPass->getID(CI);
  unsigned long retindex = matchReturnWithCall(DV.index,
                                               bbID,
                                               callID,
                                               trace[DV.index].tid);

  unsigned long tempretindex = retindex - 1;
  while (trace[tempretindex].type != RecordType::BBType)
    tempretindex--;

  if (!(trace[tempretindex].type == RecordType::BBType &&
        trace[tempretindex].tid == trace[retindex].tid &&
        trace[tempretindex].address == trace[retindex].address)) {
    errs() << "Return and BB record doesn't match! May be due to some reason "
              "the records of a called function are not recorded as in stat "
              "function of mysql.\n";
    for (unsigned index = 0; index < CI->getNumOperands(); ++index)
      if (!isa<Constant>(CI->getOperand(index))) {
        DynValue NDV = DynValue(CI->getOperand(index), DV.index);
        addToWorklist(NDV, Sources, DV);
      }
    return;
  }

  for (auto BB = CalledFunc->begin(); BB != CalledFunc->end(); ++BB) {
    if (isa<ReturnInst>(BB->getTerminator()))
      if (bbNumPass->getID(&*BB) == trace[tempretindex].id) {
        DynValue NDV = DynValue(BB->getTerminator(), tempretindex);
        addToWorklist(NDV, Sources, DV);
      }
  }
}

void TraceFile::getSourceForSelect(DynValue &DV, Worklist_t &Sources) {
  SelectInst *SI = dyn_cast<SelectInst>(DV.V);
  assert(SI && "getSourceForSelect used on non-select instruction!\n");

  if (!normalize(DV))
    return;

  unsigned selectID = lsNumPass->getID(SI);
  Function *Func = SI->getParent()->getParent();
  unsigned long selectIndex = findPreviousID(Func,
                                             DV.index,
                                             RecordType::PDType,
                                             trace[DV.index].tid,
                                             selectID);
  if (selectIndex == maxIndex) {
    errs() << __func__ << " failed to find.\n";
    return;
  }

  assert(trace[selectIndex].type == RecordType::PDType);
  assert(trace[selectIndex].id == selectID);

  unsigned predicate = trace[selectIndex].address;
  Value *Operand = predicate ? SI->getTrueValue() : SI->getFalseValue();
  DynValue NDV = DynValue(Operand, DV.index);
  addToWorklist(NDV, Sources, DV);
}