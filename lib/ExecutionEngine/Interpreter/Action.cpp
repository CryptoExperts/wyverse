#include "llvm/ExecutionEngine/Action.h"
#include "Interpreter.h"

namespace llvm {

ExecutionContext *ECStackAccessor::currentEC() {
  if (ECStack->empty()) {
    return nullptr;
  }
  return &(ECStack->back());
}

void Action::setInterpreter(Interpreter * interpreter) {
  this->interpreter = interpreter;
}

Interpreter * Action::getInterpreter() {
  return interpreter;
}

namespace SampleKind {

  // These are actually bitmasks that get or-ed together.
  enum Kind {
    MemoryRead     = 0x1,
    MemoryWrite    = 0x2,
    StackRead      = 0x4,
    StackWrite     = 0x8,
    RegisterStor   = 0x16,
  };

}


using byte = unsigned char;

struct TraceSample {
  SampleKind::Kind         kind;
  uint8_t                  size;
  SmallVector<uint8_t, 8>  data;

  TraceSample(SampleKind::Kind kind, uint8_t size, SmallVector<uint8_t, 8>  data)
    : kind(kind), size(size), data(data) {}
};


class TraceSampleFilter {

};


unsigned getAPIntNumBytes(APInt Val) {
  assert(Val.getBitWidth() % 8  == 0);
  return Val.getBitWidth() / 8;
}

void TraceProcessor::traceAPInt(APInt Val) {
  if (Val.getBitWidth() == 1) {
    outs() << Val.getBoolValue();
  } else {
    outs() << Val;
  }
  outs() << "  " << Val.getBitWidth() << "\n";
}

void TraceProcessor::trace(GenericValue GV, Type *Ty) {
  if (Ty->isVectorTy() &&
      cast<VectorType>(Ty)->getElementType()->isIntegerTy()) {
    for (unsigned i = 0; i < GV.AggregateVal.size(); ++i)
      traceAPInt(GV.AggregateVal[i].IntVal);
  } else if (Ty->isIntegerTy()) {
    traceAPInt(GV.IntVal);
  } else if (Ty->isPointerTy()) {
    dbgs() << "Unhandled type: " << *Ty << " (" << Ty->getTypeID() << ")\n";
  } else {
    dbgs() << "Unhandled type: " << *Ty << " (" << Ty->getTypeID() << ")\n";
    llvm_unreachable(nullptr);
  }
}

GenericValue TraceProcessor::getOperandValue(Value *I, ExecutionContext &SF) {
  return action->getInterpreter()->getOperandValue(I, SF);
}


void TraceProcessor::defaultVisitor(Value &Val) {
  Type *Ty  = Val.getType();
  ExecutionContext * SF = currentEC();
  GenericValue GV = getOperandValue(&Val, *SF);
  trace(GV, Ty);
}

void TraceProcessor::visitStoreInst(StoreInst &I) {
  Value * Op0= I.getOperand(0);
  defaultVisitor(*Op0) ;
}

void TraceProcessor::visitReturnInst(ReturnInst &I) {
  // return if calling the `main` function (the entry point)
  ExecutionContext *CallingSF = currentEC();
  if (!CallingSF)
    return;

  CallingSF->CurInst--;
  Instruction &CallerInst = *(CallingSF->CurInst);
  CallingSF->CurInst++;

  // only trace non-void returns
  if (!CallerInst.getType()->isVoidTy()) {
    defaultVisitor(CallerInst);
  }
}

class FaultAction : public Action {
public:
  // TODO
};


Action * ActionFactory::createAction(const char * actionType) {
  if (strcmp(actionType, "helloworld") == 0) {
    return new HelloWorldAction();
  } else if (strcmp(actionType, "trace") == 0) {
    return new TraceAction();
  } else {
    errs() << "unknown action " << actionType <<  " ignored!\n";
    return NULL;
  }
}

void TraceAction::print(raw_ostream &ROS) {
  ROS << "TraceAction => Trace memory / register while executing your binray.\n";
}

}
