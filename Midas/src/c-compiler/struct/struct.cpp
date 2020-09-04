#include <iostream>
#include <function/expressions/shared/shared.h>
#include <function/expressions/shared/weaks.h>

#include "struct.h"

#include "translatetype.h"

void declareStruct(
    GlobalState* globalState,
    StructDefinition* structM) {

  auto innerStructL =
      LLVMStructCreateNamed(
          LLVMGetGlobalContext(), structM->name->name.c_str());
  assert(globalState->innerStructs.count(structM->name->name) == 0);
  globalState->innerStructs.emplace(structM->name->name, innerStructL);

  auto wrapperStructL =
      LLVMStructCreateNamed(
          LLVMGetGlobalContext(), (structM->name->name + "rc").c_str());
  assert(globalState->wrapperStructs.count(structM->name->name) == 0);
  globalState->wrapperStructs.emplace(structM->name->name, wrapperStructL);

  auto structWeakRefStructL =
      LLVMStructCreateNamed(
          LLVMGetGlobalContext(), (structM->name->name + "w").c_str());
  assert(globalState->structWeakRefStructs.count(structM->name->name) == 0);
  globalState->structWeakRefStructs.emplace(structM->name->name, structWeakRefStructL);
}

void translateStruct(
    GlobalState* globalState,
    StructDefinition* structM) {
  LLVMTypeRef valStructL = globalState->getInnerStruct(structM->name);
  std::vector<LLVMTypeRef> innerStructMemberTypesL;
  for (int i = 0; i < structM->members.size(); i++) {
    innerStructMemberTypesL.push_back(
        translateType(
            globalState,
            structM->members[i]->type));
  }
  LLVMStructSetBody(
      valStructL, innerStructMemberTypesL.data(), innerStructMemberTypesL.size(), false);

  LLVMTypeRef wrapperStructL = globalState->getWrapperStruct(structM->name);
  std::vector<LLVMTypeRef> wrapperStructMemberTypesL;

  // First member is a ref counts struct. We don't include the int directly
  // because we want fat pointers to point to this struct, so they can reach
  // into it and increment without doing any casting.
  if (structM->mutability == Mutability::MUTABLE) {
    if (getEffectiveWeakability(globalState, structM) == Weakability::WEAKABLE) {
      wrapperStructMemberTypesL.push_back(globalState->mutWeakableControlBlockStructL);
    } else {
      wrapperStructMemberTypesL.push_back(globalState->mutNonWeakableControlBlockStructL);
    }
  } else if (structM->mutability == Mutability::IMMUTABLE) {
    wrapperStructMemberTypesL.push_back(globalState->immControlBlockStructL);
  } else assert(false);

  wrapperStructMemberTypesL.push_back(valStructL);

  LLVMStructSetBody(
      wrapperStructL, wrapperStructMemberTypesL.data(), wrapperStructMemberTypesL.size(), false);

  auto structWeakRefStructL = globalState->getStructWeakRefStruct(structM->name);
  makeStructWeakRefStruct(globalState, structWeakRefStructL, wrapperStructL);
}

void declareEdge(
    GlobalState* globalState,
    Edge* edge) {

  auto interfaceTableStructL =
      globalState->getInterfaceTableStruct(edge->interfaceName->fullName);

  auto edgeName =
      edge->structName->fullName->name + edge->interfaceName->fullName->name;
  auto itablePtr =
      LLVMAddGlobal(globalState->mod, interfaceTableStructL, edgeName.c_str());
  LLVMSetLinkage(itablePtr, LLVMExternalLinkage);

  globalState->interfaceTablePtrs.emplace(edge, itablePtr);
}

void translateEdge(
    GlobalState* globalState,
    Edge* edge) {

  auto interfaceTableStructL =
      globalState->getInterfaceTableStruct(edge->interfaceName->fullName);

  auto builder = LLVMCreateBuilder();
  auto itableLE = LLVMGetUndef(interfaceTableStructL);
  for (int i = 0; i < edge->structPrototypesByInterfaceMethod.size(); i++) {
    auto funcName = edge->structPrototypesByInterfaceMethod[i].second->name;
    itableLE = LLVMBuildInsertValue(
        builder,
        itableLE,
        globalState->getFunction(funcName),
        i,
        std::to_string(i).c_str());
  }
  LLVMDisposeBuilder(builder);

  auto itablePtr = globalState->getInterfaceTablePtr(edge);
  LLVMSetInitializer(itablePtr,  itableLE);
}
