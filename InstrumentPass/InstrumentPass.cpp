#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

enum TargetInstrumentation {
	FirstToolOnly = 0,
	Multiplex
};

enum ComposeFunction {
	DontCompose = 0,
	Chained 
};

/* These options could be made simpler to use by having a "manifest" file with all the information
   and passing it through the command line instead of having to specify all of this individually */
cl::list<std::string> tools("instrument-tools", cl::desc("<instrumentation tools>"), cl::Hidden);

cl::opt<TargetInstrumentation> targetInstrumentation("target-instrumentation", cl::desc("Type of target program instrumentation"), 
	cl::values(
		clEnumVal(FirstToolOnly, "Only instrument the target program with the first tool in the list"),
		clEnumVal(Multiplex, "Instrument the target program with all tools")),
	cl::init(TargetInstrumentation::FirstToolOnly));

cl::opt<ComposeFunction> composeFunction("compose-function", cl::desc("Type of composition function for instrumentation tools"), 
	cl::values(
		clEnumVal(DontCompose, "Only the target program is instrumented (i.e. no tool is instrumented)"),
		clEnumVal(Chained, "Each tool is istrumented by the one that immediately follows in the list")),
	cl::init(ComposeFunction::DontCompose));

namespace {
	struct InstrumentPass : public ModulePass {

		std::vector<std::string> toolNames;

		static char ID;
		InstrumentPass() : ModulePass(ID) {

			// Retrieve tool names.
			for (auto& tool : tools)
			{
				toolNames.push_back(tool);
			}

			// Nothing to multiplex if there is at most one instrumentation tool.
			if (toolNames.size() < 2)
				targetInstrumentation = TargetInstrumentation::FirstToolOnly;
		}

		// Build a multiplexer function that calls all the tools' functions (in order of appearance in the list).
		void BuildMultiplexFunction(Module& M, Function* multiplexFunction, const std::string& functionPrefix)
		{
			auto& C = M.getContext();

			multiplexFunction->setLinkage(GlobalValue::LinkOnceODRLinkage);
			multiplexFunction->setCallingConv(CallingConv::C);

			BasicBlock* block = BasicBlock::Create(C, "entry", multiplexFunction);
			IRBuilder<> builder(block);

			llvm::Function::arg_iterator args = multiplexFunction->arg_begin();
			llvm::Value* fnName = args;

			for (auto& tool : toolNames)
			{
				Function* toolInstrument = (Function*) M.getOrInsertFunction(functionPrefix + tool, FunctionType::get(Type::getVoidTy(C), {Type::getInt8Ty(C)->getPointerTo()},false));
				builder.CreateCall(toolInstrument, {fnName});
			}

			builder.CreateRetVoid();
		}

		void DefineMultiplexFunctions(Module& M)
		{
			if (targetInstrumentation == TargetInstrumentation::Multiplex)
			{
				auto& C = M.getContext();

				Function* instrumentBefore = (Function*) M.getOrInsertFunction("__instrument_before_call_multiplex", FunctionType::get(Type::getVoidTy(C), {Type::getInt8Ty(C)->getPointerTo()}, false));
				Function* instrumentAfter = (Function*) M.getOrInsertFunction("__instrument_after_call_multiplex", FunctionType::get(Type::getVoidTy(C), {Type::getInt8Ty(C)->getPointerTo()},false));

				BuildMultiplexFunction(M, instrumentBefore, "__instrument_before_call_");
				BuildMultiplexFunction(M, instrumentAfter, "__instrument_after_call_");
			}
		}

		virtual bool runOnModule(Module &M) {

			// Check that we're actually asked to instrument something.
			if (toolNames.size() == 0)
				return false;	

			auto& C = M.getContext();

			FunctionType* instrumentType =  FunctionType::get(Type::getVoidTy(C), {Type::getInt8Ty(C)->getPointerTo()},false);

			// Identify whether this translation unit is an instrumentation tool (and if so, which one).
			auto it = std::find(toolNames.begin(), toolNames.end(), M.getName().str());

			int toolIndex = -1;

			if (it != toolNames.end())
				toolIndex = std::distance(toolNames.begin(), it);

			// Get the instrumentation functions.
			Function* instrumentBefore = (Function*) M.getOrInsertFunction("__instrument_before_call", instrumentType);
			Function* instrumentAfter = (Function*) M.getOrInsertFunction("__instrument_after_call", instrumentType);

			// Get rid of bitcasts, they are introduced by extern "C" declarations but are not required here.
			instrumentBefore = (Function*)instrumentBefore->stripPointerCasts();
			instrumentAfter = (Function*)instrumentAfter->stripPointerCasts();

			// If only the first tool is used to instrument the target program, rename the functions being called.
			if (targetInstrumentation == TargetInstrumentation::FirstToolOnly && (toolIndex == -1))
			{
				instrumentBefore->setName("__instrument_before_call_" + toolNames[0]);
				instrumentAfter->setName("__instrument_after_call_" + toolNames[0]);
			}
			// If all the tools are used to instrument the target program, create a multiplexer function and use that.
			else if (targetInstrumentation == TargetInstrumentation::Multiplex && toolIndex == -1)
			{			
				DefineMultiplexFunctions(M);
				instrumentBefore = (Function*) M.getOrInsertFunction("__instrument_before_call_multiplex", instrumentType);
				instrumentAfter = (Function*) M.getOrInsertFunction("__instrument_after_call_multiplex", instrumentType);		
			}

			// For every tool, rename the instrumentation function so that they can be identified and don't clash.
			if (toolIndex != -1)
			{
				instrumentBefore->setName("__instrument_before_call_" + toolNames[toolIndex]);
				instrumentAfter->setName("__instrument_after_call_" + toolNames[toolIndex]);
			}

			// If this is a tool and we don't want to compose tools, we're done.
			if (composeFunction == ComposeFunction::DontCompose && toolIndex != -1)
			{
				return true;
			}
			// If we want to compose tools...
			else if (composeFunction == ComposeFunction::Chained && toolIndex >= 0)
			{
				if (toolIndex == toolNames.size() - 1) // Last of the chain.
				{
					return true;
				}
				else // This tool is going to be instrumented by the following tool in the chain.
				{
					instrumentBefore = (Function*) M.getOrInsertFunction("__instrument_before_call_" + toolNames[toolIndex+1] , instrumentType);
					instrumentAfter = (Function*) M.getOrInsertFunction("__instrument_after_call_" + toolNames[toolIndex+1] , instrumentType);
				}
			}

			std::vector<CallInst*> calls;

			// Retrieve all call sites to instrument.
			for (auto& F : M)
			{
				// The target program should never instrument the multiplexing functions.
				if (toolIndex == -1 && (&F == instrumentBefore || &F == instrumentAfter))
					continue;

				for (auto& B : F)
				{
					for (auto& I : B)
					{
						CallInst* call = dyn_cast<CallInst>(&I);
						if (call)
						{
							calls.push_back(call);
						}
					}
				}
			}

			IRBuilder<> builder{C};

			// Instrument all call sites.
			for (auto call : calls)
			{
				builder.SetInsertPoint(call);

				std::string fnName = "[" + M.getName().str() + "]" + call->getCalledFunction()->getName().str();

				Value* str = GetPointerToString(C,builder, fnName);

				CallInst* callBefore = builder.CreateCall(instrumentBefore,  {str});

				builder.SetInsertPoint(call->getNextNode());

				CallInst* callAfter = builder.CreateCall(instrumentAfter, {str});
			}

			return true;
		}

		// Allocate a string on the stack and get a pointer to it.
		Value* GetPointerToString(LLVMContext& C, IRBuilder<>& builder, std::string& str)
		{
			Constant* strArg = ConstantDataArray::getString(C, str);

			auto* strType = ArrayType::get(Type::getInt8Ty(C), str.size() + 1);
			Value* alloca = builder.CreateAlloca(strType);

			builder.CreateStore(strArg, alloca);
			Value* strPtr = builder.CreateBitCast(alloca, Type::getInt8Ty(C)->getPointerTo());

			return strPtr;
		}


	};
}

char InstrumentPass::ID = 0;

// Register the pass both for when no optimizations and all optimizations are enabled.
static void registerInstrumentPass(const PassManagerBuilder &,
	legacy::PassManagerBase &PM) {
	PM.add(new InstrumentPass());
}

static void registerInstrumentPassOpt(const PassManagerBuilder &,
	legacy::PassManagerBase &PM) {
	PM.add(new InstrumentPass());
}

static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
	registerInstrumentPass);

static RegisterStandardPasses
RegisterMyPassOpt(PassManagerBuilder::EP_ModuleOptimizerEarly,
	registerInstrumentPassOpt);
