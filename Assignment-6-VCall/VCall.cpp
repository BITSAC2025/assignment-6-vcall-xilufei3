/**
 * Andersen.cpp
 * @author kisslune
 */

#include "A6Header.h"

using namespace llvm;
using namespace std;
using namespace SVF;

int main(int argc, char** argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");

    SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVF::SVFIRBuilder builder;
    auto pag = builder.build();
    auto consg = new SVF::ConstraintGraph(pag);
    consg->dump();

    Andersen andersen(consg);
    auto cg = pag->getCallGraph();

    // TODO: complete the following two methods
    andersen.runPointerAnalysis();
    andersen.updateCallGraph(cg);

    cg->dump();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
	return 0;
}


void Andersen::runPointerAnalysis()
{
    // TODO: complete this method. Point-to set and worklist are defined in A5Header.h
    //  The implementation of constraint graph is provided in the SVF library
    WorkList<NodeID> worklist;
    for (auto it = consg->begin(); it != consg->end(); ++it) {
        NodeID p = it->first;
        ConstraintNode *node = it->second;
        bool changed = false;
        for (auto addrEdge : node->getAddrInEdges()) {
            NodeID o = addrEdge->getSrcID();
            if (pts[p].insert(o).second) {
                changed = true;
            }
        }
        if (changed) {
            worklist.push(p);
        }
    }
    while (!worklist.empty()) {
        NodeID p = worklist.pop();
        ConstraintNode *node = consg->getConstraintNode(p);

        for (auto o: pts[p]) {
            for (auto StoreEdge : node->getStoreInEdges()) {
                NodeID q = StoreEdge->getSrcID();
                if (consg->addCopyCGEdge(q, o))
                    worklist.push(q);
            }
            for (auto LoadEdge : node->getLoadOutEdges()) {
                NodeID r = LoadEdge->getDstID();
                if (consg->addCopyCGEdge(o, r))
                    worklist.push(o);
            }
        }

        for (auto CopyEdge : node->getCopyOutEdges()) {
            NodeID x = CopyEdge->getDstID();
            bool change = false;
            for (auto o : pts[p]) {
                if (pts[x].insert(o).second)
                    change = true;
            }
            if (change) {
                worklist.push(x);
            }
        }
        for (auto GepEdge : node->getGepOutEdges()) {
            NodeID x = GepEdge->getDstID();
            bool change = false;
            for (auto o : pts[p]) {
                APOffset apOffset = 0;
                // if (auto gepEdge = SVFUtil::dyn_cast<SVF::GepCGEdge>(GepEdge))
                if (auto gepEdge = SVFUtil::dyn_cast<SVF::NormalGepCGEdge>(GepEdge))
                    apOffset = gepEdge->getConstantFieldIdx();
                NodeID fldNode = consg->getGepObjVar(o, apOffset);
                if (pts[x].insert(fldNode).second)
                    change = true;
            }
            if (change) {
                worklist.push(x);
            }
        }
    }
}


void Andersen::updateCallGraph(SVF::CallGraph* cg)
{
    // TODO: complete this method.
    //  The implementation of call graph is provided in the SVF library
    const auto& CallSiteMap = consg->getIndirectCallsites();
    for (auto& pair: CallSiteMap) {
        const CallICFGNode* cs = pair.first;
        NodeID funPtr = pair.second;
        const FunObjVar* callerFun = cs->getCaller();

        for (NodeID tgt : pts[funPtr]) {
            if (consg->isFunction(tgt)) {
                const FunObjVar* calleeFun = consg->getFunction(tgt);
                cg->addIndirectCallGraphEdge(cs, callerFun, calleeFun);
            }
        }
    }
}