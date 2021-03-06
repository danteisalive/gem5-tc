/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Kevin Lim
 */

#ifndef __CPU_O3_POINTER_DEP_GRAPH_IMPL_HH__
#define __CPU_O3_POINTER_DEP_GRAPH_IMPL_HH__

#include "cpu/o3/comm.hh"
#include "debug/TypeTracker.hh"
#include "cpu/o3/pointer_dep_graph.hh"
#include "debug/PointerDepGraph.hh"


template <class Impl>
PointerDependencyGraph<Impl>::PointerDependencyGraph(O3CPU *_cpu )
        :isTypeTrackerEnabled(true), cpu(_cpu), numEntries(0), memAllocCounter(0), nodesTraversed(0), nodesRemoved(0)
{
        for (int i = 0; i < TheISA::NumIntRegs; ++i) {
            dependGraph[i].clear();
            FetchArchRegsPid[i] = TheISA::PointerID(0);
            CommitArchRegsPid[i] = TheISA::PointerID(0);
        }
        std::ofstream TyCHEAliasSanityCheckFile;
        TyCHEAliasSanityCheckFile.open("./m5out/AliasSanity.tyche", std::fstream::out);
        TyCHEAliasSanityCheckFile.close();

}

template <class Impl>
PointerDependencyGraph<Impl>::~PointerDependencyGraph()
{



}


template <class Impl>
void
PointerDependencyGraph<Impl>::reset()
{

    for (int i = 0; i < TheISA::NumIntRegs; ++i) {
        dependGraph[i].clear();
        FetchArchRegsPid[i] = TheISA::PointerID(0);
        CommitArchRegsPid[i] = TheISA::PointerID(0);
    }

    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::NONE;
    isTypeTrackerEnabled = true;
}

template <class Impl>
void
PointerDependencyGraph<Impl>::insert(DynInstPtr &inst)
{
    InternalUpdate(inst, true);   
}

template <class Impl>
void
PointerDependencyGraph<Impl>::doSquash(const DynInstPtr squashedInst, uint64_t squashedSeqNum){

   
    DPRINTF(PointerDepGraph, "Squashing Alias Until Sequence Number: [%d]\n", squashedSeqNum);
    if (squashedInst)
        DPRINTF(PointerDepGraph, "[%d][%s][%s][Inst. PID: %s][AIT: %s][%d][%s]\n", 
                                squashedInst->seqNum,
                                squashedInst->pcState(), 
                                squashedInst->staticInst->disassemble(squashedInst->pcState().instAddr()),
                                squashedInst->dyn_pid,
                                (squashedInst->isAliasInTransition() ? "Yes" : "No"),
                                (squashedInst->isAliasInTransition() ? squashedInst->getAliasStoreSeqNum() : 0),
                                (squashedInst->isTypeTracked() ? "T" : "NT")
                                );
    DPRINTF(PointerDepGraph, "Dependency Graph Before Squashing:\n");
    dump();




    // if the producer seqNum is greater than squashedSeqNum then
    // remove it and all of the consumers as they are all will be squashed
    uint64_t    firstInstSeqNum = UINT64_MAX;
    bool        firstInstState  = false;
    bool        isAPMicroop     = false;
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {

        // Erase all even numbers (C++11 and later)
        for (auto it = dependGraph[i].begin(); it != dependGraph[i].end(); )
        {
            if (it->inst->seqNum > squashedSeqNum) {
                // book keeping
                if (it->inst->seqNum < firstInstSeqNum) 
                {
                    firstInstSeqNum =  it->inst->seqNum;
                    firstInstState  =  it->inst->isTypeTracked();
                    // all of the APs are injected. bounds check are not in dep graph
                    isAPMicroop     =  it->inst->isMicroopInjected(); 
                } 
            
                it->inst = NULL;
                it = dependGraph[i].erase(it);
            } else {
                ++it;
            }
        }

    } // for loop

    // if the depGraph is empty, then we need to fall back to another method for determining type tracker state
    if (!updateTypeTrackerState())
    {
        assert((firstInstSeqNum != UINT64_MAX) && 
            "Unexptected state when squashing! This means there was no inst to squash and depGraph is also empty!\n");
        assert(!isAPMicroop && "What should we do about this?!\n");
        
        isTypeTrackerEnabled = firstInstState;
    }
    

    // now for each int reg update the FetchArchRegsPid with the front inst
    // if there is no inst in the queue then update it with the
    // CommitArchRegsPid
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {
        if (dependGraph[i].empty()){
          FetchArchRegsPid[i] = CommitArchRegsPid[i];
        }
        else {
          FetchArchRegsPid[i] = dependGraph[i].front().pid;
        }
    }

    DPRINTF(PointerDepGraph, "Dependency Graph After Squashing:\n");
    dump();
}


// return false if the depGraph is empty
template <class Impl>
bool
PointerDependencyGraph<Impl>::updateTypeTrackerState()
{
    // in order to put type tracker in a consistent state, we go through all the in transit insts 
    // and find the latest state
    // depending on the last instruction, 
    // if we are on the borders, malloc/calloc/free size and base then they determine next state
    // else just look at the IsTypeTracker flag


    uint64_t lastSeqNum = 0;
    bool found = false;
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {

        // Erase all even numbers (C++11 and later)
        for (auto it = dependGraph[i].cbegin(); it != dependGraph[i].cend(); ++it)
        {
            if (it->inst->seqNum > lastSeqNum) 
            {
                lastSeqNum = it->inst->seqNum;
                found = true;
                if (it->inst->isMallocSizeCollectorMicroop())
                {
                    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::MALLOC_SIZE;
                    isTypeTrackerEnabled = false;
                } 
                else if (it->inst->isCallocSizeCollectorMicroop())
                {
                    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE;
                    isTypeTrackerEnabled = false;
                }
                else if (it->inst->isReallocSizeCollectorMicroop())
                {   
                    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::REALLOC_SIZE;
                    isTypeTrackerEnabled = false;
                } 
                else if (it->inst->isFreeCallMicroop())
                {
                    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::FREE_CALL;
                    isTypeTrackerEnabled = false;
                } 
                else if (it->inst->isMallocBaseCollectorMicroop() || 
                        it->inst->isCallocBaseCollectorMicroop() || 
                        it->inst->isReallocBaseCollectorMicroop() ||
                        it->inst->isFreeRetMicroop())
                {
                    // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::NONE;
                    isTypeTrackerEnabled = true;
                }
                else 
                {
                    // just check the status of the last inst
                    isTypeTrackerEnabled = it->inst->isTypeTracked();
                }
            } 
        }

    } // for loop
    
    assert(((found && lastSeqNum != 0) || (!found && lastSeqNum == 0)) && "Not a viable state!\n");

    // if the depGraph is emoty, fall back to another method to determine type tracker state
    return (lastSeqNum != 0) ? true : false;

}

template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerRefillLoad(DynInstPtr &inst, TheISA::PointerID pid)
{
    inst->macroop->setMacroopPid(pid);
    inst->staticInst->setStaticPointerID(pid);
    inst->dyn_pid = pid;

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    assert(inst_regop != nullptr && "inst_regop is null!\n");
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 && "Invalid load refill size!\n");

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;//index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); src1 = src1;//base
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest

    for (auto it = dependGraph[dest].begin(); it != dependGraph[dest].end(); it++)
    {
        if (it->inst->seqNum == inst->seqNum) 
        {
            assert(it->inst->isLoad() && 
                    "updatePointerRefillLoad:: doUpdate is callled for non-load microop!\n");
            it->pid = inst->dyn_pid;
            
            DPRINTF(PointerDepGraph, "updatePointerRefillLoad:: Updated Pointer Refill: [%d][%s][%s][Dyn: %s][Static: %s][Dep.: %s]\n", 
                        it->inst->seqNum,
                        it->inst->pcState(), 
                        it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                        it->inst->dyn_pid,
                        it->inst->staticInst->getStaticPointerID(),
                        it->pid
                        );




            return;
        }

    } // for loop

    assert("Cannot find a load refill that should be in the dependency graph!\n");

}

template <class Impl>
void
PointerDependencyGraph<Impl>::syncPointerRefills(DynInstPtr &inst)
{
        assert(inst->isStore() && "syncPointerRefills for non-store microop!\n");

        for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) 
        {
            // Erase  (C++11 and later)
            for (auto it = dependGraph[indx].begin(); it != dependGraph[indx].end(); it++)
            {
                //if found: update and return
                if (it->inst->isAliasInTransition() && 
                    (it->inst->getAliasStoreSeqNum() == inst->seqNum) &&
                    (it->inst->dyn_pid != inst->dyn_pid)) 
                {

                    DPRINTF(PointerDepGraph, "Load Refill Instruction is not in Sync: [%d][%s][%s][Dyn: %s][Static: %s][Dep.: %s][Macro: %s]\n", 
                        it->inst->seqNum,
                        it->inst->pcState(), 
                        it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                        it->inst->dyn_pid,
                        it->inst->staticInst->getStaticPointerID(),
                        it->pid,
                        it->inst->macroop->getMacroopPid()
                        );

                    it->pid = inst->dyn_pid;
                    it->inst->dyn_pid = inst->dyn_pid;
                    it->inst->staticInst->setStaticPointerID(inst->dyn_pid);
                    it->inst->macroop->setMacroopPid(inst->dyn_pid);
                    assert(it->inst->isLoad() && "syncPointerRefills:: doUpdate is callled for non-load microop!\n");

                    DPRINTF(PointerDepGraph, "Load Refill Instruction After Sync: [%d][%s][%s][Dyn: %s][Static: %s][Dep.: %s][Macro: %s]\n", 
                        it->inst->seqNum,
                        it->inst->pcState(), 
                        it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                        it->inst->dyn_pid,
                        it->inst->staticInst->getStaticPointerID(),
                        it->pid,
                        it->inst->macroop->getMacroopPid()
                        );

                    doUpdate(it->inst);
                }
                else if (it->inst->isAliasInTransition() && 
                        (it->inst->getAliasStoreSeqNum() == inst->seqNum) &&
                        (it->inst->dyn_pid == inst->dyn_pid)) 
                {
                    dump();
                    assert((it->pid == inst->dyn_pid) && "DependGraph and Inst. are not in Sync!\n");
                }
            }

        } // for loop
}


template <class Impl>
void
PointerDependencyGraph<Impl>::updatePIDWithTypeTracker(DynInstPtr &inst, ThreadContext* tc)
{
    if (inst->isStore()) {updatePointerTrackerForStoreMicroop(inst); return;}
    else if (inst->isBoundsCheckMicroop()) {updateBoundsCheckMicroop(inst);return;}
    // some instructions need to be verifed at the exec stage to make sure that their 
    // propagations are correct. If not correct, we will run an update. This is modeled by a 
    // n cycle delay in exec
    // This is only should be done for add/addi with size 8, as other versions are assumed to be zero always
    else if (inst->staticInst->getName() == "add")  {updatePointerTrackerForAddMicroop(inst, tc); return;}
    else if (inst->staticInst->getName() == "sub")  {updatePointerTrackerForSubMicroop(inst, tc); return;}
    else if (inst->staticInst->getName() == "lea") {updatePointerTrackerForLeaMicroop(inst, tc); return;}
    else 
    {
        return;
    }

}


template <class Impl>
void
PointerDependencyGraph<Impl>::doCommit(DynInstPtr &inst){

    if (inst->isBoundsCheckMicroop()) return; // we do not save these


    DPRINTF(PointerDepGraph, "Commiting Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(PointerDepGraph, "Dependency Graph Before Commiting:\n");
    dump();

    // here set the final static PID as this is getting commited!
    inst->staticInst->setStaticPointerID(inst->dyn_pid);
    // for all the dest regs for this inst, commit it
    // assert if the inst is not in the dependGraph

    for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) 
    {
        if (inst->destRegIdx(i).isIntReg())
        {
            X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
            uint16_t dest_reg_idx = x86_inst->getUnflattenRegIndex(inst->staticInst->destRegIdx(i)); 
            
            panic_if(dependGraph[dest_reg_idx].back().inst->seqNum !=
                    inst->seqNum,
                    "Dangling inst in PointerDependGraph");

            // before commiting, perform a sanity check
            PerformSanityCheck(inst);

            assert(dependGraph[dest_reg_idx].back().pid == inst->dyn_pid && 
                "dependGraph and inst dynamic pid are not consistent!\n");

            // by here we should have the PID and TID both valid!
            assert(dependGraph[dest_reg_idx].back().pid.GetTypeID() == inst->dyn_pid.GetTypeID() && 
                "dependGraph and inst dynamic tid are not consistent!\n");

            DPRINTF(PointerDepGraph, "Setting: CommitArchRegsPid[%d]=%s\n",
                    TheISA::IntRegIndexStr(dest_reg_idx), dependGraph[dest_reg_idx].back().pid);

            CommitArchRegsPid[dest_reg_idx] = dependGraph[dest_reg_idx].back().pid;

            // zero out all interface regs for the next macroopp
            if (dependGraph[dest_reg_idx].back().inst->isLastMicroop())
            {
                DPRINTF(PointerDepGraph, "Last Microop in Macroop! Setting all T[n] regs to PID(0)!\n");
                for (size_t i = X86ISA::NUM_INTREGS; i < TheISA::NumIntRegs; i++) {
                    //zero out all dest regs
                    CommitArchRegsPid[i] = TheISA::PointerID(0);
                }
            }

            dependGraph[dest_reg_idx].back().inst = NULL;
            dependGraph[dest_reg_idx].pop_back();
        }
    }
   
    DPRINTF(PointerDepGraph, "Dependency Graph After Commiting:\n");
    dump();
}


template <class Impl>
void
PointerDependencyGraph<Impl>::PerformSanityCheck(DynInstPtr &inst)
{

    if (inst->isBoundsCheckMicroop()) return;


    if ((inst->isMallocBaseCollectorMicroop() ||
        inst->isCallocBaseCollectorMicroop()))
    {
        assert(inst->dyn_pid != TheISA::PointerID(0) && 
                "isM/CallocBaseCollectorMicroop :: Found a Malloc/Calloc Call Microop with zero PID Return Value!\n");
    }
    else if (inst->isMallocSizeCollectorMicroop())
    {
        assert(inst->dyn_pid  == TheISA::PointerID(0) &&
                "isMallocSizeCollectorMicroop :: Found a Malloc/Calloc Microop with non-zero  PID Parameter!\n");
    }
    else if (inst->isCallocSizeCollectorMicroop()) 
    {
        
        assert(inst->dyn_pid  == TheISA::PointerID(0) &&
                "isCallocSizeCollectorMicroop :: Found a Malloc/Calloc Microop with non-zero PID Parameter!\n");
    }
    else if (inst->isReallocSizeCollectorMicroop())
    {

        assert(inst->dyn_pid  != TheISA::PointerID(0) &&
                "isreallocSizeCollectorMicroop :: Found a Realloc Microop with non-zero RDI PID Parameter!\n");

        assert(CommitArchRegsPid[X86ISA::INTREG_RDI] != TheISA::PointerID(0) && 
                "Found a Realloc microop with zero pid for RDI!\n");
        assert(CommitArchRegsPid[X86ISA::INTREG_RSI] == TheISA::PointerID(0) && 
                "Found a Realloc microop with non-zero pid for RSI!\n");
    }
    else if (inst->isReallocBaseCollectorMicroop())
    {
        assert(inst->dyn_pid != TheISA::PointerID(0) && 
                "isM/CallocBaseCollectorMicroop :: Found a Realloc Call Microop with zero PID Return Value!\n");  
    }
    else if (inst->isFreeCallMicroop())
    {
        assert(inst->dyn_pid  != TheISA::PointerID(0) && 
                "isFreeCallMicroop :: Found a Free Call Microop with zero PID Parameter!\n");
    }
    else if (inst->isFreeRetMicroop())
    {
        assert(inst->dyn_pid  == TheISA::PointerID(0) &&
                "isFreeRetMicroop :: Found a Free Ret Microop with non-zero PID Parameter!\n");
    }
    else if (inst->staticInst->getName() == "mov")          {TransferMovMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "lea")          {TransferLeaMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "add")          {TransferAddMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "sub")          {TransferSubMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "addi")         {TransferAddImmMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "subi")         {TransferSubImmMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "and")          {TransferAndMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "andi")         {TransferAndImmMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "xor")          {TransferXorMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "xori")         {TransferXorImmMicroops(inst, false, true);}
    else if (inst->isLoad())
    {
        if (inst->staticInst->getName() == "ld")                {TransferLoadMicroops(inst, false, true);}
        else if (inst->staticInst->getName() == "ldis")         {TransferLoadInStackMicroops(inst, false, true);}
        else if (inst->staticInst->getName() == "ldst" 
                || inst->staticInst->getName() == "ldstl")      {TransferLoadStoreMicroops(inst, false, true);}
        else if (inst->staticInst->getName() == "ldsplit"
                || inst->staticInst->getName() == "ldsplitl")   {TransferLoadSplitMicroops(inst, false, true);} 
        else 
        {
            // name them
            panic_if(inst->staticInst->getName() != "ldfp" && 
                    inst->staticInst->getName() != "ldfp87" &&
                    inst->staticInst->getName() != "ldifp87", 
                    "Unknown Load Type: %s\n", inst->staticInst->getName());

            // make sure thier index reg is PID(0)
            assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
            TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
            const uint8_t dataSize = inst_regop->dataSize;
            assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

            X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

            uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index


            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a Floating Point Microop with non-zero PID Index!\n");
                
            panic_if(inst->dyn_pid != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a Floating Point Microop with non-zero PID Dest!\n");

            
        }
    } 
    else if (inst->isStore())
    {
        if (inst->staticInst->getName() == "st")                    {TransferStoreMicroops(inst, false, true);}
        else if (inst->staticInst->getName() == "stis")             {TransferStoreInStackMicroops(inst, false, true);}
        else if (inst->staticInst->getName() == "stul")             {TransferStoreUnsignedLongMicroops(inst, false, true);} 
        else if (inst->staticInst->getName() == "stsplit"
                || inst->staticInst->getName() == "stsplitul")      {TransferStoreSplitMicroops(inst, false, true);} 
        else 
        {
            // name them
            panic_if(inst->staticInst->getName() != "stfp" && 
                    inst->staticInst->getName() != "stfp87" &&
                    inst->staticInst->getName() != "cda" &&
                    inst->staticInst->getName() != "clflushopt" && 
                    inst->staticInst->getName() != "clwb", 
                    "Unknown Store Type: %s\n", inst->staticInst->getName());
            // make sure thier index reg is PID(0)
            assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
            TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
            const uint8_t dataSize = inst_regop->dataSize;
            assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

            X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

            uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index


            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a Floating Point Microop with non-zero PID Index!\n");
                
            panic_if(inst->dyn_pid != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a Floating Point Microop with non-zero PID Dest!\n");

        }
          
    }
    else {

        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
            if (inst->destRegIdx(i).isIntReg())
            {
                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
                uint16_t dest_reg_idx = x86_inst->getUnflattenRegIndex(inst->staticInst->destRegIdx(i)); 
                dest_reg_idx = dest_reg_idx;
                // what do need to check here?! 

                panic_if(inst->dyn_pid != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a \"%s\" Microop with non-zero PID Dest!\n",
                        inst->staticInst->getName());
            }
        }

    }



    //snapshot
    // for (size_t i = 0; i < TheISA::NumIntRegs; i++) {
    //     inst->CommitArchRegsPid[i] = CommitArchRegsPid[i];
    // }

    return;

}

template <class Impl>
void
PointerDependencyGraph<Impl>::dump(DynInstPtr &inst)
{

    
    for (size_t i = 0; i < TheISA::NumIntRegs; i++)
    {
        if (FetchArchRegsPid[i] != TheISA::PointerID(0) || CommitArchRegsPid[i] != TheISA::PointerID(0))
            DPRINTF(PointerDepGraph, "FetchArchRegsPid[%s]\t=\t[%d]\t\tCommitArchRegsPid[%s]\t=\t[%d]\n", 
                    TheISA::IntRegIndexStr(i), FetchArchRegsPid[i], TheISA::IntRegIndexStr(i), CommitArchRegsPid[i]);
    }
    

    DPRINTF(PointerDepGraph, "---------------------------------------------------------------------\n");

}

template <class Impl>
void
PointerDependencyGraph<Impl>::dump()
{
    
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {

        for (auto it = dependGraph[i].begin(); it != dependGraph[i].end(); it++)
        {
          assert(it->inst); // shouldnt be a null inst
          DPRINTF(PointerDepGraph, "[%s] ==> [%d][%s][%s][Inst: %s][Dep: %s][AIT: %s][%d][%s]\n", 
                                TheISA::IntRegIndexStr(i),
                                it->inst->seqNum,
                                it->inst->pcState(), 
                                it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                                it->inst->dyn_pid,
                                it->pid, 
                                (it->inst->isAliasInTransition() ? "Yes" : "No"),
                                (it->inst->isAliasInTransition() ? it->inst->getAliasStoreSeqNum() : 0),
                                (it->inst->isTypeTracked() ? "T" : "NT")
                                );
     
        }
    }

    for (size_t i = 0; i < TheISA::NumIntRegs; i++)
    {
        if (FetchArchRegsPid[i] != TheISA::PointerID(0) || CommitArchRegsPid[i] != TheISA::PointerID(0))
            DPRINTF(PointerDepGraph, "FetchArchRegsPid[%s]\t=\t[%d]\t\tCommitArchRegsPid[%s]\t=\t[%d]\n", 
                    TheISA::IntRegIndexStr(i), FetchArchRegsPid[i], TheISA::IntRegIndexStr(i), CommitArchRegsPid[i]);
    }
    DPRINTF(PointerDepGraph, "---------------------------------------------------------------------\n");

}




template <class Impl>
bool 
PointerDependencyGraph<Impl>::checkTyCHESanity(DynInstPtr& head_inst, ThreadContext* tc)
{

    
    if (head_inst->isLoad())
    {
        // for now just do it for the aliases
        if (head_inst->staticInst->getName() != "ld" && 
            head_inst->staticInst->getName() != "ldis") return true;

        assert(head_inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
        TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )head_inst->staticInst.get(); 
        assert(inst_regop != nullptr && "inst_regop is null!\n"); 
        const uint8_t dataSize = inst_regop->dataSize;
        assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

        X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)head_inst->staticInst.get();

        uint16_t src0 = x86_inst->getUnflattenRegIndex(head_inst->srcRegIdx(0)); src0 = src0;//index
        uint16_t src1 = x86_inst->getUnflattenRegIndex(head_inst->srcRegIdx(1)); src1 = src1;//base
        uint16_t dest = x86_inst->getUnflattenRegIndex(head_inst->destRegIdx(0)); //dest

        // check whether this is a pointer refill and whether it's tracking the right PID/TID
        if (dataSize == 8)
        {
            // read the data
            uint64_t  dataRegContent = 
                    head_inst->readDestReg(head_inst->staticInst.get(), 0); 
            TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 

            DPRINTF(PointerDepGraph, "checkTyCHESanity:: Checking Alias for dataRegContent[%s]=%x\n", 
                    TheISA::IntRegIndexStr(dest), dataRegContent);


            Trace::InstRecord * trace = head_inst->traceData;
            Trace::ExeTracerRecord * execTraceRecord = dynamic_cast<Trace::ExeTracerRecord*>(trace);
            assert(execTraceRecord && "null trace is found!\n");

            if ((_pid != TheISA::PointerID(0)) || (head_inst->dyn_pid != TheISA::PointerID(0)))
            {   
                std::ofstream TyCHEAliasSanityCheckFile;
                // File Open
                TyCHEAliasSanityCheckFile.open("./m5out/AliasSanity.tyche", std::ios_base::app);
                TyCHEAliasSanityCheckFile << "---------------------------------------\n";
                TyCHEAliasSanityCheckFile << _pid << " " << head_inst->dyn_pid << "\n"; 
                
                if ((_pid != head_inst->dyn_pid))
                    TyCHEAliasSanityCheckFile << "Failed to verify that alias and dynamic pid are the same!\n";

                if (execTraceRecord)
                    execTraceRecord->dump(TyCHEAliasSanityCheckFile);
                
                // File Close
                TyCHEAliasSanityCheckFile << "---------------------------------------\n";
                TyCHEAliasSanityCheckFile.close();
                // this is only true when we are actualy outside of the APs
                if (head_inst->isTypeTracked())
                    assert((_pid == head_inst->dyn_pid) &&
                        "Failed to verify that alias and dynamic pid are the same!\n");
            }

            

        }

        return true;
        // Boulds check could be any size
        // check bounds check sanity!

    }
    else if (head_inst->isStore())
    {
        // for now just do it for the aliases
        if (head_inst->staticInst->getName() != "st" && 
            head_inst->staticInst->getName() != "stis") return true;

        assert(head_inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
        TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )head_inst->staticInst.get(); 
        assert(inst_regop != nullptr && "inst_regop is null!\n"); 
        const uint8_t dataSize = inst_regop->dataSize;
        assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

        X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)head_inst->staticInst.get();

        uint16_t src0 = x86_inst->getUnflattenRegIndex(head_inst->srcRegIdx(0)); src0 = src0;//index
        uint16_t src1 = x86_inst->getUnflattenRegIndex(head_inst->srcRegIdx(1)); src1 = src1;//base
        uint16_t dest = x86_inst->getUnflattenRegIndex(head_inst->srcRegIdx(2)); dest = dest; //dest

        // check whether this is a pointer refill and whether it's tracking the right PID/TID
        if (dataSize == 8)
        {
            // read the data
            uint64_t  dataRegContent = 
                head_inst->readIntRegOperand(head_inst->staticInst.get(), 2); // src(2) is the data register
            
            TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 

            DPRINTF(PointerDepGraph, "checkTyCHESanity:: Checking Alias for dataRegContent[%s]=%x\n", 
                    TheISA::IntRegIndexStr(dest), dataRegContent);


            Trace::InstRecord * trace = head_inst->traceData;
            Trace::ExeTracerRecord * execTraceRecord = dynamic_cast<Trace::ExeTracerRecord*>(trace);
            assert(execTraceRecord && "null trace is found!\n");

            if ((_pid != TheISA::PointerID(0)) || (head_inst->dyn_pid != TheISA::PointerID(0)))
            {   
                std::ofstream TyCHEAliasSanityCheckFile;
                // File Open
                TyCHEAliasSanityCheckFile.open("./m5out/AliasSanity.tyche", std::ios_base::app);
                TyCHEAliasSanityCheckFile << "---------------------------------------\n";
                TyCHEAliasSanityCheckFile << _pid << " " << head_inst->dyn_pid << "\n"; 
                
                if ((_pid != head_inst->dyn_pid))
                    TyCHEAliasSanityCheckFile << "Failed to verify that alias and dynamic pid are the same!\n";

                if (execTraceRecord)
                    execTraceRecord->dump(TyCHEAliasSanityCheckFile);
                
                // File Close
                TyCHEAliasSanityCheckFile << "---------------------------------------\n";
                TyCHEAliasSanityCheckFile.close();
                // this is only true when we are actualy outside of the APs
                if (head_inst->isTypeTracked())
                    assert((_pid == head_inst->dyn_pid) &&
                        "Failed to verify that alias and dynamic pid are the same for the store microop!\n");
            }

            

        }

        return true;
        // Boulds check could be any size
        // check bounds check sanity!

    }




    return true;
}


template <class Impl>
TheISA::PointerID
PointerDependencyGraph<Impl>::readPIDFromIntervalTree(Addr vaddr, ThreadContext* tc)
{


    Block fake;
    fake.payload = vaddr;
    fake.req_szB = 1;
    UWord foundkey = 1;
    UWord foundval = 1;
    unsigned char found = VG_lookupFM( tc->interval_tree,
                                        &foundkey, &foundval, (UWord)&fake );
    if (!found) {
        return TheISA::PointerID(0);
    }

    assert(foundval == 0 && "foundval == 0\n");  
    assert(foundkey != 1 && "foundkey != 1\n");
    Block* res = (Block*)foundkey;
    assert(res != &fake && "res != &fake\n");
    assert(res->pid != 0 && "res->pid == 0 ");
    assert(res->tid != 0 && "res->tid == 0");

    return TheISA::PointerID(res->pid, res->tid);

}


template <class Impl>
void
PointerDependencyGraph<Impl>::doUpdate(DynInstPtr& inst)
{
    

    if (inst->isBoundsCheckMicroop()) return; // we do not save these
    assert (inst->isLoad() && "doUpdate called with a non-load instruction!") ;
    assert((inst->staticInst->getName() == "ld" || inst->staticInst->getName() == "ldis") && 
            "Not a ld or ldis instruction called with doUpdate()\n");
    assert(inst->staticInst->getDataSize() == 8 && "data size is not 8!");

    DPRINTF(PointerDepGraph, "Updating Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(PointerDepGraph, "Dependency Graph Before Updating:\n");
    dump();


    //find the load uop
    // we should defenitly find it othersie it's a panic!
    // doUpdate happends before squash so we should find it in the DEP Graph
    bool found = false;
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) 
    {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].begin(); it != dependGraph[indx].end(); it++)
        {
            //if found: update and return
            if (it->inst->seqNum == inst->seqNum) {
                // get the actual PID for this load
                TheISA::PointerID _pid = inst->macroop->getMacroopPid();
                // insert an entry for the destination reg
                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
                uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest
                inst->FetchArchRegsPid[dest] = _pid;
                it->pid = _pid;
                found = true;
                break;

            }
        }

    } // for loop
    if (!found) {
        return; // in the process of squashing.
    }

    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        FetchArchRegsPid[indx] = inst->FetchArchRegsPid[indx];
    }

    DPRINTF(PointerDepGraph, "FetchArchRegsPids Reg Before Update:\n");
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        DPRINTF(PointerDepGraph, "FetchArchRegsPids[%s]  = %s\n", 
                TheISA::IntRegIndexStr(indx) , FetchArchRegsPid[indx]);
    }

    std::map<uint64_t, uint64_t> MicroopsToUpdate;
    // order the microops that are tracked after this pointer refill
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].cbegin(); it != dependGraph[indx].cend(); it++)
        {
            if (it->inst->seqNum > inst->seqNum) {
                MicroopsToUpdate.insert(std::make_pair(it->inst->seqNum, indx));
            }
        }
    }

    // now one by one find all the instruction with seqNum greater
    // than load uop and update their uops in order!
    for (auto& microops : MicroopsToUpdate)
    {
        uint64_t SeqNumber = microops.first;
        uint64_t Indx = microops.second;
        for (auto it = dependGraph[Indx].begin(); it != dependGraph[Indx].end(); it++)
        {
            if (it->inst->seqNum == SeqNumber) {
                InternalUpdate(it->inst, false);
                it->pid = it->inst->dyn_pid;
                break;
            }
        }
    }



    DPRINTF(PointerDepGraph, "Dependency Graph After Updating:\n");
    dump();


}

template <class Impl>
void
PointerDependencyGraph<Impl>::InternalUpdate(DynInstPtr &inst, bool track)
{

    if (inst->isBoundsCheckMicroop()) return;

    // the type tracker is disabled during APs because Ap's are assumed to be safe and 
    // we cannot track anything inside them as they do many OOB accesses
    if (track) 
    {
        inst->setTypeTracked(isTypeTrackerEnabled);
    }


    DPRINTF(PointerDepGraph, "%s Alias for Instruction: [%d][%s][%s]\n", 
            track ? "Tracking" : "Updating",
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));

    DPRINTF(PointerDepGraph, "Dependency Graph Before %s:\n", 
            track ? "Tracking" : "Updating");
    dump();


    if ((track) && 
        (inst->isMallocBaseCollectorMicroop() ||
        inst->isCallocBaseCollectorMicroop()))
    {
        // here we generate a new PID and insert it into rax
        dependGraph[X86ISA::INTREG_RAX].push_front(
                                        PointerDepEntry(inst, inst->dyn_pid));
        FetchArchRegsPid[X86ISA::INTREG_RAX] = inst->dyn_pid;

        DPRINTF(PointerDepGraph, " Enabling Type Tracker! Malloc/Calloc base collector is called! Assigned PID=%s!\n", 
                FetchArchRegsPid[X86ISA::INTREG_RAX]);
        
        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::NONE;
        isTypeTrackerEnabled = true;
    }
    else if (track && inst->isMallocSizeCollectorMicroop())
    {

        dependGraph[X86ISA::INTREG_RDI].push_front(
                                        PointerDepEntry(inst, TheISA::PointerID(0)));
        FetchArchRegsPid[X86ISA::INTREG_RDI] = TheISA::PointerID(0);
        inst->dyn_pid = FetchArchRegsPid[X86ISA::INTREG_RDI];
        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::MALLOC_SIZE;
        isTypeTrackerEnabled = false;

        DPRINTF(PointerDepGraph, "Malloc/Calloc size collector is called! Disabling the type tracker!\n");
    }
    else if (track && inst->isCallocSizeCollectorMicroop())
    {
        dependGraph[X86ISA::INTREG_RDI].push_front(
                                        PointerDepEntry(inst, TheISA::PointerID(0)));
        FetchArchRegsPid[X86ISA::INTREG_RDI] = TheISA::PointerID(0);
        inst->dyn_pid = FetchArchRegsPid[X86ISA::INTREG_RDI];
        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE;
        isTypeTrackerEnabled = false;

        DPRINTF(PointerDepGraph, "Malloc/Calloc size collector is called! Disabling the type tracker!\n");
    }
    else if ((track) && inst->isReallocBaseCollectorMicroop())
    {
        // here we generate a new PID and insert it into rax
        dependGraph[X86ISA::INTREG_RAX].push_front(PointerDepEntry(inst, inst->dyn_pid));
        FetchArchRegsPid[X86ISA::INTREG_RAX] = inst->dyn_pid;

        DPRINTF(PointerDepGraph, " Enabling Type Tracker! Realloc base collector is called! Assigned PID=%s!\n", 
                FetchArchRegsPid[X86ISA::INTREG_RAX]);
        
        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::NONE;
        isTypeTrackerEnabled = true;
    }
    else if ((track) &&  inst->isReallocSizeCollectorMicroop())
    {
        dependGraph[X86ISA::INTREG_RDI].push_front(PointerDepEntry(inst, FetchArchRegsPid[X86ISA::INTREG_RDI]));
        inst->dyn_pid = FetchArchRegsPid[X86ISA::INTREG_RDI];

        isTypeTrackerEnabled = false;

        DPRINTF(PointerDepGraph, "Realloc size collector is called! Disabling the type tracker!\n");
    }
    else if ((track) && inst->isFreeCallMicroop())
    {
        dependGraph[X86ISA::INTREG_RDI].push_front(PointerDepEntry(inst, FetchArchRegsPid[X86ISA::INTREG_RDI]));
        inst->dyn_pid = FetchArchRegsPid[X86ISA::INTREG_RDI];
        // RDI (parameter) and R10 and R11 should be nulled uopn calling free!
        DPRINTF(PointerDepGraph, "Disabling Type Tracker! Free is called! Invalidating PID=%s\n", 
                FetchArchRegsPid[X86ISA::INTREG_RDI]);

        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE;
        isTypeTrackerEnabled = false;

    }
    else if ((track) && inst->isFreeRetMicroop())
    {
        assert(inst->dyn_pid == TheISA::PointerID(0) && "Dynamic PID is zero for FREE RET!\n");
        dependGraph[X86ISA::INTREG_RDI].push_front(PointerDepEntry(inst, inst->dyn_pid));
        // which registers should be set to zero?
        FetchArchRegsPid[X86ISA::INTREG_RDI] = inst->dyn_pid;
        // RDI (parameter) and R10 and R11 should be nulled uopn calling free!
        DPRINTF(PointerDepGraph, "Free is returning! Invalidating PID=%s\n", 
                FetchArchRegsPid[X86ISA::INTREG_RDI]);

        // typeTrackerStatus = ThreadContext::COLLECTOR_STATUS::NONE;
        isTypeTrackerEnabled = true;
    }
    else if (inst->staticInst->getName() == "mov")  {TransferMovMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "lea")  {TransferLeaMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "st")   {TransferStoreMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "stis") {TransferStoreInStackMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "ld")   {TransferLoadMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "ldis") {TransferLoadInStackMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "add")  {TransferAddMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "sub")  {TransferSubMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "addi") {TransferAddImmMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "subi") {TransferSubImmMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "and")  {TransferAndMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "andi")  {TransferAndImmMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "xor")  {TransferXorMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "xori")  {TransferXorImmMicroops(inst, track, false);}
    else {

        TheISA::PointerID _pid{0} ;
        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) 
        {
            if (inst->destRegIdx(i).isIntReg())
            {
                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
                uint16_t dest_reg_idx = x86_inst->getUnflattenRegIndex(inst->staticInst->destRegIdx(i)); 

                if (track)
                {
                    dependGraph[dest_reg_idx].push_front(PointerDepEntry(inst, _pid));
                }

                FetchArchRegsPid[dest_reg_idx] = _pid;
            }
        }


    }


    // zero out all interface regs for the next macroopp
    if (inst->isLastMicroop()){
      for (size_t i = X86ISA::NUM_INTREGS; i < TheISA::NumIntRegs; i++) {
          //zero out all dest regs
          FetchArchRegsPid[i] = TheISA::PointerID(0);
      }
    }

    //snapshot
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {
        inst->FetchArchRegsPid[i] = FetchArchRegsPid[i];
    }

    DPRINTF(PointerDepGraph, "Dependency Graph After %s:\n", 
            track ? "Tracking" : "Updating");
    dump();


}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferLeaMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest


    
    X86ISAInst::Lea * inst_lea = dynamic_cast<X86ISAInst::Lea*>(inst->staticInst.get()); 
    assert((inst_lea != nullptr) && "Found a 8 bytes lea inst that is not X86ISA::Lea!\n");


    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 8 Lea Microop with non-zero PID Index!\n");
        return;
    }    

    TheISA::PointerID _pid = TheISA::PointerID(0);

    if (dataSize == 8)
    {
        _pid = FetchArchRegsPid[src1];
    }

    FetchArchRegsPid[dest] = _pid;
    inst->dyn_pid = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }
            

    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s]=FetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(dest),
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest].GetPointerID());

    

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferMovMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Mov * inst_mov = dynamic_cast<X86ISAInst::Mov*>(inst->staticInst.get()); 
    X86ISAInst::MovFlags * inst_mov_flags = dynamic_cast<X86ISAInst::MovFlags*>(inst->staticInst.get()); 
    if (inst_mov != nullptr || inst_mov_flags != nullptr)
    {

            X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

            uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;
            uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
            uint16_t dest = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2));

            // this is at commit to make sure everything is right! Don't do anything!
            if (sanity)
            {
                if ((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                    CommitArchRegsPid[src1] != TheISA::PointerID(0))
                {
                    assert(inst->dyn_pid == TheISA::PointerID(0) && 
                          "Found a 1/2/4 bytes Mov Inst with non-zero PID!\n");
                }
                return;
            }


            DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== FetchArchRegsPid[%s]=[%d]\n", 
                TheISA::IntRegIndexStr(dest),
                TheISA::IntRegIndexStr(src1), 
                FetchArchRegsPid[src1].GetPointerID());


            TheISA::PointerID _pid = TheISA::PointerID(0);
            if (dataSize == 8)
            {
                _pid = FetchArchRegsPid[src1];
            }
            else 
            {
                // sometimes the source operand can be a pointer in a 1/2/4 mov but we know that 
                // dest register is always 0
                _pid = TheISA::PointerID(0);
            }
            
            FetchArchRegsPid[dest] = _pid;

            if (track)
            {
                dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
            }

            inst->dyn_pid = _pid;
            
    }
    else 
    {
        assert(false && "Found a mov inst that is not Mov or MovFlags!\n");
    }
}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferStoreMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::St * inst_st = dynamic_cast<X86ISAInst::St*>(inst->staticInst.get()); 
    assert(inst_st != nullptr && "Found a st inst that is not X86ISA::St!\n");
 

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
    uint16_t src2 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2)); //data



    DPRINTF(PointerDepGraph, "\t\tMEM[Base(%s), Index(%s)] <=== FetchArchRegsPid[%s]=[%d]\n", 
                TheISA::IntRegIndexStr(src1),
                TheISA::IntRegIndexStr(src0),
                TheISA::IntRegIndexStr(src2), 
                FetchArchRegsPid[src2].GetPointerID());
    
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && (CommitArchRegsPid[src2] != TheISA::PointerID(0)),
                            "TransferStoreMicroops :: Found a 1/2/4 bytes St Inst with data reg non-zero PID!\n");

        panic_if (!(inst->staticInst->getSegment() == TheISA::SEGMENT_REG_DS || inst->staticInst->getSegment() == TheISA::SEGMENT_REG_SS) 
                        && (CommitArchRegsPid[src2] != TheISA::PointerID(0)), 
                        "TransferStoreMicroops :: Found a St Inst with SS || DS segment reg that data reg is not PID(0)!\n");

        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferStoreMicroops :: Found a St Microop with non-zero PID Index!\n");
        return;
    }

        
            

    TheISA::PointerID _pid = FetchArchRegsPid[src2];

    inst->dyn_pid = _pid;
            
    
 

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferStoreInStackMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Stis * inst_stis = dynamic_cast<X86ISAInst::Stis*>(inst->staticInst.get()); 
    assert(inst_stis != nullptr && "Found a stis inst that is not X86ISA::Stis!\n");
 

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
    uint16_t src2 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2)); //data



    DPRINTF(PointerDepGraph, "\t\tMEM[Base(%s), Index(%s)] <=== FetchArchRegsPid[%s]=[%d]\n", 
                TheISA::IntRegIndexStr(src1),
                TheISA::IntRegIndexStr(src0),
                TheISA::IntRegIndexStr(src2), 
                FetchArchRegsPid[src2].GetPointerID());
    
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && (CommitArchRegsPid[src2] != TheISA::PointerID(0)),
                            "TransferStoreMicroops :: Found a 1/2/4 bytes Stis Inst with data reg non-zero PID!\n");

        panic_if (!(inst->staticInst->getSegment() == TheISA::SEGMENT_REG_DS || inst->staticInst->getSegment() == TheISA::SEGMENT_REG_SS) 
                        && (CommitArchRegsPid[src2] != TheISA::PointerID(0)), 
                        "TransferStoreMicroops :: Found a Stis Inst with SS || DS segment reg that data reg is not PID(0)!\n");

        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferStoreMicroops :: Found a Stis Microop with non-zero PID Index!\n");
        return;
    }

        
            

    TheISA::PointerID _pid = FetchArchRegsPid[src2];

    inst->dyn_pid = _pid;
            
    
 

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferLoadMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest

    // This is 1/2 bytes Load Microop -- This should never be pointer refill!
    if (dataSize == 2 || dataSize == 1)
    {
        X86ISAInst::Ld * inst_ld = dynamic_cast<X86ISAInst::Ld*>(inst->staticInst.get()); 
        assert(inst_ld != nullptr && "Found a 1/2 bytes ld inst that is not X86ISA::Ld!\n");

        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {
           panic_if(inst_regop->getScale() != 1 && 
                    CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                    "TransferLoadeMicroops :: Found a 1/2 Ld Microop with non-zero PID Index! SRC[%s] = %s SRC[%s] = %s Scale = %u\n",
                    TheISA::IntRegIndexStr(src0), CommitArchRegsPid[src0],
                    TheISA::IntRegIndexStr(src1), CommitArchRegsPid[src1], 
                    inst_regop->getScale());
            return;
        }
       
                

        FetchArchRegsPid[dest] = TheISA::PointerID(0);

        inst->dyn_pid = TheISA::PointerID(0);

        if (track) 
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, TheISA::PointerID(0)));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest].GetPointerID());

        
    }

    else if ( dataSize == 4)
    {
        X86ISAInst::LdBig * inst_ldbig = dynamic_cast<X86ISAInst::LdBig*>(inst->staticInst.get()); 
        assert((inst_ldbig != nullptr) && "Found a 4 bytes ld inst that is not X86ISA::LdBig!\n");




        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {
            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 4 Ld Microop with non-zero PID Index!\n");
            return;
        }    
                

        FetchArchRegsPid[dest] = TheISA::PointerID(0);

        inst->dyn_pid = TheISA::PointerID(0);

        if (track)
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, TheISA::PointerID(0)));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest].GetPointerID());


    }
    else 
    {
        X86ISAInst::LdBig * inst_ldbig = dynamic_cast<X86ISAInst::LdBig*>(inst->staticInst.get()); 
        assert((inst_ldbig != nullptr) && "Found a 8 bytes ld inst that is not X86ISA::LdBig!\n");


        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {
            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 8 Ld Microop with non-zero PID Index!\n");
            return;
        }    
                
        TheISA::PointerID _pid = inst->macroop->getMacroopPid();
        FetchArchRegsPid[dest] = _pid;

        inst->dyn_pid = _pid;

        if (track)
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest].GetPointerID());

    }

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferLoadInStackMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest

    // This is 1/2 bytes Load Microop -- This should never be pointer refill!
    if (dataSize == 2 || dataSize == 1)
    {
        X86ISAInst::Ldis * inst_ldis = dynamic_cast<X86ISAInst::Ldis*>(inst->staticInst.get()); 
        assert(inst_ldis != nullptr && "Found a 1/2 bytes ldis inst that is not X86ISA::Ldis!\n");


        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {
            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 1/2 Ldis Microop with non-zero PID Index!\n");
            return;
        }   
                

        FetchArchRegsPid[dest] = TheISA::PointerID(0);

        inst->dyn_pid = TheISA::PointerID(0);

        if (track) 
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, TheISA::PointerID(0)));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest]);

        
    }

    else if ( dataSize == 4)
    {
        X86ISAInst::LdisBig * inst_ldisbig = dynamic_cast<X86ISAInst::LdisBig*>(inst->staticInst.get()); 
        assert((inst_ldisbig != nullptr) && "Found a 4 bytes ldis inst that is not X86ISA::LdisBig!\n");



        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {      
            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 4 bytes Ldis Microop with non-zero PID Index!\n");
            return;
        }    
                

        FetchArchRegsPid[dest] = TheISA::PointerID(0);

        inst->dyn_pid = TheISA::PointerID(0);

        if (track)
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, TheISA::PointerID(0)));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%d]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest].GetPointerID());


    }
    else 
    {
        X86ISAInst::LdisBig * inst_ldisbig = dynamic_cast<X86ISAInst::LdisBig*>(inst->staticInst.get()); 
        assert((inst_ldisbig != nullptr) && "Found an 8 bytes ldis inst that is not X86ISA::LdisBig!\n");


        // this is at commit to make sure everything is right! Don't do anything!
        if (sanity)
        {        
            panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found an 8 bytes Ldis Microop with non-zero PID Index!\n");
            return;
        }    
                
        TheISA::PointerID _pid = inst->macroop->getMacroopPid();
        FetchArchRegsPid[dest] = _pid;

        inst->dyn_pid = _pid;

        if (track)
        {
            dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
        }
            

        DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[BASE(%s) + INDEX(%s)]=[%s]\n", 
                    TheISA::IntRegIndexStr(src1),
                    TheISA::IntRegIndexStr(src0), 
                    FetchArchRegsPid[dest]);

    }

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferAddMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Add * inst_add = dynamic_cast<X86ISAInst::Add*>(inst->staticInst.get()); 
    X86ISAInst::AddBig * inst_add_big = dynamic_cast<X86ISAInst::AddBig*>(inst->staticInst.get()); 
    
    X86ISAInst::AddFlags * inst_add_flags = dynamic_cast<X86ISAInst::AddFlags*>(inst->staticInst.get());
    X86ISAInst::AddFlagsBig * inst_add_flags_big = dynamic_cast<X86ISAInst::AddFlagsBig*>(inst->staticInst.get());

    assert((inst_add != nullptr || inst_add_big != nullptr ||
            inst_add_flags != nullptr || inst_add_flags_big != nullptr) 
            && "Found an add inst that is not Add/AddBig or AddFlags/AddFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) || CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes Add Inst with non-zero PID sources! " 
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        panic_if((dataSize == 8) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "TransferStoreMicroops :: Found an Add inst with both regs non-zero PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] + SRC2Pid[%d][%s]\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0],
            TheISA::IntRegIndexStr(src1), 
            FetchArchRegsPid[src1]);

    
    TheISA::PointerID _pid = (FetchArchRegsPid[src0] != TheISA::PointerID(0)) ?  FetchArchRegsPid[src0] : FetchArchRegsPid[src1];

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferSubMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Sub * inst_sub = dynamic_cast<X86ISAInst::Sub*>(inst->staticInst.get()); 
    X86ISAInst::SubBig * inst_sub_big = dynamic_cast<X86ISAInst::SubBig*>(inst->staticInst.get()); 
    
    X86ISAInst::SubFlags * inst_sub_flags = dynamic_cast<X86ISAInst::SubFlags*>(inst->staticInst.get());
    X86ISAInst::SubFlagsBig * inst_sub_flags_big = dynamic_cast<X86ISAInst::SubFlagsBig*>(inst->staticInst.get());

    assert((inst_sub != nullptr || inst_sub_big != nullptr ||
            inst_sub_flags != nullptr || inst_sub_flags_big != nullptr) 
            && "Found an sub inst that is not Sub/SubBig or SubFlags/SubFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        // We should never have a pointer in a 1/2/4 bytes register
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) || CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes Sub Inst with non-zero PID sources! " 
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);



        // for SubFlag/SubFlagBig the output should always be zero
        // if((dataSize == 8) &&
        //     (inst_sub_flags != nullptr || inst_sub_flags_big != nullptr))
        // {
        //     if (
        //         (
        //             CommitArchRegsPid[src1] != TheISA::PointerID(0) && 
        //             CommitArchRegsPid[src0] != TheISA::PointerID(0) && 
        //             CommitArchRegsPid[src1] != CommitArchRegsPid[src0]
        //         ) || 
        //         (
        //             CommitArchRegsPid[src1] != TheISA::PointerID(0) && 
        //             CommitArchRegsPid[src0] == TheISA::PointerID(0)
        //         ) || 
        //         (
        //             CommitArchRegsPid[src0] != TheISA::PointerID(0) && 
        //             CommitArchRegsPid[src1] == TheISA::PointerID(0)
        //         )
        //         )
        //     {
        //         std::ofstream TyCHEAliasSanityCheckFile;
        //         TyCHEAliasSanityCheckFile.open("./m5out/AliasSanity.tyche", std::ios_base::app);
        //         TyCHEAliasSanityCheckFile << "---------------------------------------\n";
                
        //         TyCHEAliasSanityCheckFile << "Failed to verify TransferSubInst!\n";
        //         TyCHEAliasSanityCheckFile << 
        //             "TransferSubMicroops :: Found a SubFlag* inst with both regs non-zero PID and not equal!\n" <<
        //             "SeqNum: " << inst->seqNum << " " << 
        //             "PC: " << inst->pcState() << " " << 
        //             "Inst: " << inst->staticInst->disassemble(inst->pcState().instAddr()) << " " << 
        //             "SRC1 =" << CommitArchRegsPid[src0] << " " <<
        //             "SRC2 = " << CommitArchRegsPid[src1] << "\n";
            
        //         // File Close
        //         TyCHEAliasSanityCheckFile << "---------------------------------------\n";
        //         TyCHEAliasSanityCheckFile.close();
        //     }


        // } 


        //These are just for normal sub instructions as their result affect other int regs
        // this is not acceptable for Sub/SubBig
        panic_if((dataSize == 8) &&
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && 
                CommitArchRegsPid[src0] != TheISA::PointerID(0) &&
                CommitArchRegsPid[src1] != CommitArchRegsPid[src0]), 
                "TransferSubMicroops :: Found a Sub inst with both src regs non-zero non-equal PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);
        // dest = src0(PID(0)) - src1(PID(n))
        panic_if((dataSize == 8) && 
                (inst_sub != nullptr || inst_sub_big != nullptr) &&
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && 
                CommitArchRegsPid[src0] == TheISA::PointerID(0)), 
                "TransferSubMicroops :: Found a Sub inst with src1 reg non-zero PID but src0 reg zeo PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] - SRC2Pid[%d][%s]\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0],
            TheISA::IntRegIndexStr(src1), 
            FetchArchRegsPid[src1]);

    TheISA::PointerID _pid = TheISA::PointerID(0);

    // if both have equal PID, then the result for sure is PID(0)
    if (FetchArchRegsPid[src0] != FetchArchRegsPid[src1])
        _pid = (FetchArchRegsPid[src0] != TheISA::PointerID(0)) ?  FetchArchRegsPid[src0] : FetchArchRegsPid[src1];
    

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferAddImmMicroops(DynInstPtr &inst, bool track, bool sanity)
{



    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOpImm * inst_regop = (TheISA::RegOpImm * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert((inst_regop != nullptr) && (dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1));

    X86ISAInst::AddImm * inst_add_imm = dynamic_cast<X86ISAInst::AddImm*>(inst->staticInst.get()); 
    X86ISAInst::AddImmBig * inst_add_imm_big = dynamic_cast<X86ISAInst::AddImmBig*>(inst->staticInst.get()); 
    
    X86ISAInst::AddFlagsImm * inst_add_imm_flags = dynamic_cast<X86ISAInst::AddFlagsImm*>(inst->staticInst.get());
    X86ISAInst::AddFlagsImmBig * inst_add_imm_flags_big = dynamic_cast<X86ISAInst::AddFlagsImmBig*>(inst->staticInst.get());

    assert((inst_add_imm != nullptr || inst_add_imm_big != nullptr ||
            inst_add_imm_flags != nullptr || inst_add_imm_flags_big != nullptr) 
            && "Found an AddImm inst that is not AddImm/AddImmBig or AddImmFlags/AddImmFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes AddImm Inst with non-zero PID source! " 
                "SRC1 = %s\n", CommitArchRegsPid[src0]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] + Imm\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0]);

    
    TheISA::PointerID _pid = FetchArchRegsPid[src0];

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
   
}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferSubImmMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOpImm * inst_regop = (TheISA::RegOpImm * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert((inst_regop != nullptr) && (dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1));


    X86ISAInst::SubImm * inst_sub_imm = dynamic_cast<X86ISAInst::SubImm*>(inst->staticInst.get()); 
    X86ISAInst::SubImmBig * inst_sub_imm_big = dynamic_cast<X86ISAInst::SubImmBig*>(inst->staticInst.get()); 
    
    X86ISAInst::SubFlagsImm * inst_sub_imm_flags = dynamic_cast<X86ISAInst::SubFlagsImm*>(inst->staticInst.get());
    X86ISAInst::SubFlagsImmBig * inst_sub_imm_flags_big = dynamic_cast<X86ISAInst::SubFlagsImmBig*>(inst->staticInst.get());

    assert((inst_sub_imm != nullptr || inst_sub_imm_big != nullptr ||
            inst_sub_imm_flags != nullptr || inst_sub_imm_flags_big != nullptr) 
            && "Found an SubImm inst that is not SubImm/SubImmBig or SubImmFlags/SubImmFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes SubImm Inst with non-zero PID source! " 
                "SRC1 = %s\n", CommitArchRegsPid[src0]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] - Imm\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0]);

    
    TheISA::PointerID _pid = FetchArchRegsPid[src0];

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
   
}

template <class Impl>
void
PointerDependencyGraph<Impl>::TransferLoadStoreMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base 
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest
    src1 = src1; dest = dest;

    if (sanity)
    {
        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a LdSt Microop with non-zero PID Index!\n");
        
        panic_if(inst->dyn_pid != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a LdSt Microop with non-zero PID Dest!\n");
        
        return;
    }
}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferLoadSplitMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 2 && "Invalid number of dest regs!\n");
    TheISA::LdStSplitOp * inst_regop = (TheISA::LdStSplitOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base 
    uint16_t dest0 = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest_low
    uint16_t dest1 = x86_inst->getUnflattenRegIndex(inst->destRegIdx(1)); //dest_high
    src1 = src1; dest0 = dest0; dest1 = dest1;

    if (sanity)
    {
        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a LdSplit Microop with non-zero PID Index!\n");
        
        panic_if(inst->dyn_pid != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a LdSplit Microop with non-zero PID dest!\n");
    
        return;
    }
}



template <class Impl>
void
PointerDependencyGraph<Impl>::TransferStoreUnsignedLongMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base 
    uint16_t dest0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2)); //dest
    src1 = src1; 

    if (sanity)
    {
        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a Stul Microop with non-zero PID Index!\n");
        
        panic_if(CommitArchRegsPid[dest0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a Stul Microop with non-zero PID Low Dest!\n");
   
        return;
    }
}



template <class Impl>
void
PointerDependencyGraph<Impl>::TransferStoreSplitMicroops(DynInstPtr &inst, bool track, bool sanity)
{

    assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
    TheISA::LdStSplitOp * inst_regop = (TheISA::LdStSplitOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); //index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base 
    uint16_t dest0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2)); //dest low
    uint16_t dest1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(3)); //dest high
    src1 = src1; 

    if (sanity)
    {
        panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a StSplit Microop with non-zero PID Index!\n");
        
        panic_if(CommitArchRegsPid[dest0] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a StSplit Microop with non-zero PID Low Dest!\n");
   
        panic_if(CommitArchRegsPid[dest1] != TheISA::PointerID(0), 
                "TransferLoadeMicroops :: Found a StSplit Microop with non-zero PID High Dest!\n");
        return;
    }
}



template <class Impl>
void
PointerDependencyGraph<Impl>::TransferAndMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::And * inst_and = dynamic_cast<X86ISAInst::And*>(inst->staticInst.get()); 
    X86ISAInst::AndBig * inst_and_big = dynamic_cast<X86ISAInst::AndBig*>(inst->staticInst.get()); 
    
    X86ISAInst::AndFlags * inst_and_flags = dynamic_cast<X86ISAInst::AndFlags*>(inst->staticInst.get());
    X86ISAInst::AndFlagsBig * inst_and_flags_big = dynamic_cast<X86ISAInst::AndFlagsBig*>(inst->staticInst.get());

    assert((inst_and != nullptr || inst_and_big != nullptr ||
            inst_and_flags != nullptr || inst_and_flags_big != nullptr) 
            && "Found an and inst that is not And/AndBig or AndFlags/AndFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        // panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
        //         (CommitArchRegsPid[src1] != TheISA::PointerID(0) || CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
        //         "Found a 1/2/4 bytes And Inst with non-zero PID sources! " 
        //         "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        panic_if((dataSize == 8) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && CommitArchRegsPid[src0] != TheISA::PointerID(0)) &&
                (CommitArchRegsPid[src1] != CommitArchRegsPid[src0]), 
                "TransferStoreMicroops :: Found an And inst with both regs that have no equal non-zero PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] & SRC2Pid[%d][%s]\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0],
            TheISA::IntRegIndexStr(src1), 
            FetchArchRegsPid[src1]);

    TheISA::PointerID _pid = TheISA::PointerID(0);
    // And/AndBig result can be a pointer. AndFlag and AndFlagBig is always not a pointer
    if (inst_and != nullptr || inst_and_big != nullptr)
    {
        _pid = (FetchArchRegsPid[src0] != TheISA::PointerID(0)) ?  FetchArchRegsPid[src0] : FetchArchRegsPid[src1];
    }

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}




template <class Impl>
void
PointerDependencyGraph<Impl>::TransferAndImmMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOpImm * inst_regop = (TheISA::RegOpImm * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::AndImm * inst_and_imm = dynamic_cast<X86ISAInst::AndImm*>(inst->staticInst.get()); 
    X86ISAInst::AndImmBig * inst_and_imm_big = dynamic_cast<X86ISAInst::AndImmBig*>(inst->staticInst.get()); 
    
    X86ISAInst::AndFlagsImm * inst_and_imm_flags = dynamic_cast<X86ISAInst::AndFlagsImm*>(inst->staticInst.get());
    X86ISAInst::AndFlagsImmBig * inst_and_imm_flags_big = dynamic_cast<X86ISAInst::AndFlagsImmBig*>(inst->staticInst.get());

    assert((inst_and_imm != nullptr || inst_and_imm_big != nullptr ||
            inst_and_imm_flags != nullptr || inst_and_imm_flags_big != nullptr) 
            && "Found an and inst that is not AndImm/AndImmBig or AndImmFlags/AndImmFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes AndImm Inst with non-zero PID sources! " 
                "SRC1 = %s\n", CommitArchRegsPid[src0]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] & Imm\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0]);

    
    TheISA::PointerID _pid = FetchArchRegsPid[src0];

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}




template <class Impl>
void
PointerDependencyGraph<Impl>::TransferXorMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Xor * inst_xor = dynamic_cast<X86ISAInst::Xor*>(inst->staticInst.get()); 
    X86ISAInst::XorBig * inst_xor_big = dynamic_cast<X86ISAInst::XorBig*>(inst->staticInst.get()); 
    
    X86ISAInst::XorFlags * inst_xor_flags = dynamic_cast<X86ISAInst::XorFlags*>(inst->staticInst.get());
    X86ISAInst::XorFlagsBig * inst_xor_flags_big = dynamic_cast<X86ISAInst::XorFlagsBig*>(inst->staticInst.get());

    assert((inst_xor != nullptr || inst_xor_big != nullptr ||
            inst_xor_flags != nullptr || inst_xor_flags_big != nullptr) 
            && "Found an and inst that is not Xor/XorBig or XorFlags/XorFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if(( dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) || CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes Xor Inst with non-zero PID sources! " 
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);


        panic_if((dataSize == 4 || dataSize == 8) && 
                (CommitArchRegsPid[src1] != CommitArchRegsPid[src0]), 
                "TransferStoreMicroops :: Found an Xor inst with both regs that have no equal non-zero PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);
        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] ^ SRC2Pid[%d][%s]\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0],
            TheISA::IntRegIndexStr(src1), 
            FetchArchRegsPid[src1]);

    
    TheISA::PointerID _pid = TheISA::PointerID(0);

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferXorImmMicroops(DynInstPtr &inst, bool track, bool sanity)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOpImm * inst_regop = (TheISA::RegOpImm * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::XorImm * inst_xor_imm = dynamic_cast<X86ISAInst::XorImm*>(inst->staticInst.get()); 
    X86ISAInst::XorImmBig * inst_xor_imm_big = dynamic_cast<X86ISAInst::XorImmBig*>(inst->staticInst.get()); 
    
    X86ISAInst::XorFlagsImm * inst_xor_imm_flags = dynamic_cast<X86ISAInst::XorFlagsImm*>(inst->staticInst.get());
    X86ISAInst::XorFlagsImmBig * inst_xor_imm_flags_big = dynamic_cast<X86ISAInst::XorFlagsImmBig*>(inst->staticInst.get());

    assert((inst_xor_imm != nullptr || inst_xor_imm_big != nullptr ||
            inst_xor_imm_flags != nullptr || inst_xor_imm_flags_big != nullptr) 
            && "Found an and inst that is not XorImm/XorImmBig or XorImmFlags/XorImmFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0));

            
            
    // this is at commit to make sure everything is right! Don't do anything!
    if (sanity)
    {
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes XorImm Inst with non-zero PID sources! " 
                "SRC1 = %s\n", CommitArchRegsPid[src0]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] ^ Imm\n", 
            TheISA::IntRegIndexStr(dest),
            TheISA::IntRegIndexStr(src0),
            FetchArchRegsPid[src0]);

    
    TheISA::PointerID _pid = TheISA::PointerID(0);

    FetchArchRegsPid[dest] = _pid;

    if (track)
    {
        dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
    }

    inst->dyn_pid = _pid;
            

}


template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerTrackerForStoreMicroop(DynInstPtr &inst)
{

        assert((inst->staticInst->getName() == "st" || inst->staticInst->getName() == "stis") && 
                "updatePointerTrackerForStoreMicroop is called fro a store that is not st/stis\n");
        assert(inst->numIntDestRegs() == 0 && "Invalid number of dest regs!\n");
        TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
        const uint8_t dataSize = inst_regop->dataSize;
        assert(dataSize == 8 && "dataSize is not equal to 8!\n");

        X86ISAInst::St * inst_st = dynamic_cast<X86ISAInst::St*>(inst->staticInst.get()); 
        X86ISAInst::Stis * inst_stis = dynamic_cast<X86ISAInst::Stis*>(inst->staticInst.get()); 
        assert((inst_st != nullptr || inst_stis != nullptr) && "Found a st inst that is not X86ISA::St or X86ISA::Stis!\n");
    


        X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

        uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;//index
        uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); src1 = src1;//base
        uint16_t src2 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2)); //data
        
        DPRINTF(PointerDepGraph, "Store Instruction Before Update: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid,
                inst->staticInst->getStaticPointerID());

        inst->dyn_pid = CommitArchRegsPid[src2];
        inst->staticInst->setStaticPointerID(CommitArchRegsPid[src2]);

        DPRINTF(PointerDepGraph, "Updated PID for Store Instruction: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid,
                inst->staticInst->getStaticPointerID());

        // now go through all the pointer refills that use this pointer spill and make sure they are in sync!
        syncPointerRefills(inst);
}

template <class Impl>
void
PointerDependencyGraph<Impl>::updateBoundsCheckMicroop(DynInstPtr &inst)
{
        assert(inst->isLoad() && "A non-load bounds chec k microop!\n");
        assert(inst->staticInst->getName() == "ld" && "A non-ld bounds check!\n");
        assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
        TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
        const uint8_t dataSize = inst_regop->dataSize;
        assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

        X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

        uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;//index
        uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); //base
        uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); dest = dest;//dest

        DPRINTF(PointerDepGraph, "Updating PID for Bounds Check Instruction: [%d][%s][%s][%s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid);
        DPRINTF(PointerDepGraph, "Selected Dependency Graph:\n");
        dump();

        // if (dependGraph[src1].empty()) {
        //     DPRINTF(PointerDepGraph, "Dependency Graph is Empty for this Base: %s\n", TheISA::IntRegIndexStr(src1));
        //     return;
        // }
        //if (inst->seqNum > dependGraph[src1].back().inst->seqNum) return inst->dyn_pid;
        //look at the latest value of PID for base register
        TheISA::PointerID res = inst->dyn_pid;
        bool found = false;
        for (auto it = dependGraph[src1].cbegin(); it != dependGraph[src1].cend(); it++)
        {
            if (inst->seqNum > it->inst->seqNum) {
                res = it->pid;
                found  = true;
            } 
            else {
                break;
            }
        }
        // if we can't find anything, it means the commit version is valid and is the latest
        if (!found)
        {
            res = CommitArchRegsPid[src1];
        }
        inst->dyn_pid = res;
        inst->staticInst->setStaticPointerID(res);

        DPRINTF(PointerDepGraph, "Updated PID for Bounds Check Instruction: [%d][%s][%s][%s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid);

}



template <class Impl>
void
PointerDependencyGraph<Impl>::doUpdateForNoneLoadMicroops(DynInstPtr& inst)
{
    

    assert((inst->staticInst->getName() == "add" || 
            inst->staticInst->getName() == "lea" || 
            inst->staticInst->getName() == "sub") && 
            "Not a valid instruction called with doUpdateForNonLoadMicroops()\n");
    assert(inst->staticInst->getDataSize() == 8 && "data size is not 8!");

    DPRINTF(PointerDepGraph, "doUpdateForNonLoadMicroops:: Updating Pointer Tracker for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(PointerDepGraph, "doUpdateForNonLoadMicroops:: Dependency Graph Before Updating:\n");
    dump();


    // find the desginated uop
    // we should defenitly find it othersie it's a panic!
    // doUpdate happends before squash so we should find it in the DEP Graph
    bool found = false;
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) 
    {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].begin(); it != dependGraph[indx].end(); it++)
        {
            //if found: update and return
            if (it->inst->seqNum == inst->seqNum) {
                // get the actual PID for this load
                TheISA::PointerID _pid = inst->macroop->getMacroopPid();
                // insert an entry for the destination reg
                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
                uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); //dest
                inst->FetchArchRegsPid[dest] = _pid;
                it->pid = _pid;
                found = true;
                break;

            }
        }

    } // for loop
    // this is called at the commit stage, there is no way that the microop is squashed!
    assert(found && "Cannot find the designated microop!\n");

    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        FetchArchRegsPid[indx] = inst->FetchArchRegsPid[indx];
    }

    DPRINTF(PointerDepGraph, "FetchArchRegsPids Reg Before Update:\n");
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        DPRINTF(PointerDepGraph, "FetchArchRegsPids[%s]  = %s\n", 
                TheISA::IntRegIndexStr(indx) , FetchArchRegsPid[indx]);
    }

    std::map<uint64_t, uint64_t> MicroopsToUpdate;
    // order the microops that are tracked after this pointer refill
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].cbegin(); it != dependGraph[indx].cend(); it++)
        {
            if (it->inst->seqNum > inst->seqNum) {
                MicroopsToUpdate.insert(std::make_pair(it->inst->seqNum, indx));
            }
        }
    }

    // now one by one find all the instruction with seqNum greater
    // than load uop and update their uops in order!
    for (auto& microops : MicroopsToUpdate)
    {
        uint64_t SeqNumber = microops.first;
        uint64_t Indx = microops.second;
        for (auto it = dependGraph[Indx].begin(); it != dependGraph[Indx].end(); it++)
        {
            if (it->inst->seqNum == SeqNumber) {
                InternalUpdate(it->inst, false);
                it->pid = it->inst->dyn_pid;
                break;
            }
        }
    }



    DPRINTF(PointerDepGraph, "doUpdateForNonLoadMicroops::Dependency Graph After Updating:\n");
    dump();


}


template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerTrackerForSubMicroop(DynInstPtr &inst, ThreadContext* tc)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);
    
    // no need to update this add microops as they should return PID(0)!
    if (dataSize == 4 || dataSize == 2 || dataSize == 1) return;

    X86ISAInst::Sub * inst_sub = dynamic_cast<X86ISAInst::Sub*>(inst->staticInst.get()); 
    X86ISAInst::SubBig * inst_sub_big = dynamic_cast<X86ISAInst::SubBig*>(inst->staticInst.get()); 
    
    X86ISAInst::SubFlags * inst_sub_flags = dynamic_cast<X86ISAInst::SubFlags*>(inst->staticInst.get());
    X86ISAInst::SubFlagsBig * inst_sub_flags_big = dynamic_cast<X86ISAInst::SubFlagsBig*>(inst->staticInst.get());

    assert((inst_sub != nullptr || inst_sub_big != nullptr ||
            inst_sub_flags != nullptr || inst_sub_flags_big != nullptr) 
            && "Found an sub inst that is not Sub/SubBig or SubFlags/SubFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); src1 = src1;
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); dest = dest;

       
    DPRINTF(PointerDepGraph, "Sub Instruction Before Update: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()),
            inst->dyn_pid,
            inst->staticInst->getStaticPointerID());


    // read the data
    uint64_t  dataRegContent = inst->readDestReg(inst->staticInst.get(), 0); 
    
    // this is logically equal to a comparison
    TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 

    DPRINTF(PointerDepGraph, "updatePointerTrackerForAddMicroop:: Checking Alias for dataRegContent[%s]=%x=%s\n", 
                            TheISA::IntRegIndexStr(dest), dataRegContent, _pid);

    // our assumption is that all the add instructions always transfer thier sources to thier 
    // destinations. Sometimes this is not true and the result is PID(0) although the sources are PID(n)
    // in this case we need to update the pointer tracker 
    if (_pid == TheISA::PointerID(0) && inst->dyn_pid != _pid)
    {
        DPRINTF(PointerDepGraph, "updatePointerTrackerForAddMicroop:: Update is needed as Sub microop is not synced! inst->dyn_pid%s != %s\n", 
                            inst->dyn_pid, _pid);
        inst->dyn_pid = _pid;
        inst->staticInst->setStaticPointerID(_pid);  
        doUpdateForNoneLoadMicroops(inst);  
    }

    DPRINTF(PointerDepGraph, "Updated PID for Sub Instruction: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid,
                inst->staticInst->getStaticPointerID()); 
            
            

}

template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerTrackerForAddMicroop(DynInstPtr &inst, ThreadContext* tc)
{


    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    // no need to update this add microops as they should return PID(0)!
    if (dataSize == 4 || dataSize == 2 || dataSize == 1) return;

    X86ISAInst::Add * inst_add = dynamic_cast<X86ISAInst::Add*>(inst->staticInst.get()); 
    X86ISAInst::AddBig * inst_add_big = dynamic_cast<X86ISAInst::AddBig*>(inst->staticInst.get()); 
    
    X86ISAInst::AddFlags * inst_add_flags = dynamic_cast<X86ISAInst::AddFlags*>(inst->staticInst.get());
    X86ISAInst::AddFlagsBig * inst_add_flags_big = dynamic_cast<X86ISAInst::AddFlagsBig*>(inst->staticInst.get());

    assert((inst_add != nullptr || inst_add_big != nullptr ||
            inst_add_flags != nullptr || inst_add_flags_big != nullptr) 
            && "Found an add inst that is not Add/AddBig or AddFlags/AddFlagsBig!\n");


    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); src1 = src1;
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); dest = dest;


        
    DPRINTF(PointerDepGraph, "Add Instruction Before Update: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()),
            inst->dyn_pid,
            inst->staticInst->getStaticPointerID());


    // read the data
    uint64_t  dataRegContent = inst->readDestReg(inst->staticInst.get(), 0); 
    
    // this is logically equal to a comparison
    TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 

    DPRINTF(PointerDepGraph, "updatePointerTrackerForAddMicroop:: Checking Alias for dataRegContent[%s]=%x=%s\n", 
                            TheISA::IntRegIndexStr(dest), dataRegContent, _pid);

    // our assumption is that all the add instructions always transfer thier sources to thier 
    // destinations. Sometimes this is not true and the result is PID(0) although the sources are PID(n)
    // in this case we need to update the pointer tracker 
    if (_pid == TheISA::PointerID(0) && inst->dyn_pid != _pid)
    {
        DPRINTF(PointerDepGraph, "updatePointerTrackerForAddMicroop:: Update is needed as Add microop is not synced! inst->dyn_pid%s != %s\n", 
                            inst->dyn_pid, _pid);
        inst->dyn_pid = _pid;
        inst->staticInst->setStaticPointerID(_pid);  
        doUpdateForNoneLoadMicroops(inst);  
    }

    DPRINTF(PointerDepGraph, "Updated PID for Add Instruction: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid,
                inst->staticInst->getStaticPointerID()); 
            
            

}


template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerTrackerForLeaMicroop(DynInstPtr &inst, ThreadContext* tc)
{
    
    assert(inst->numIntDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::LdStOp * inst_regop = (TheISA::LdStOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4);

    // All 4 byes Lea microops return PID(0)
    if (dataSize == 4) return;

    X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

    uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); src0 = src0;//index
    uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1)); src1 = src1;//base
    uint16_t dest = x86_inst->getUnflattenRegIndex(inst->destRegIdx(0)); dest = dest;//dest


    
    X86ISAInst::Lea * inst_lea = dynamic_cast<X86ISAInst::Lea*>(inst->staticInst.get()); 
    assert((inst_lea != nullptr) && "Found a 8 bytes lea inst that is not X86ISA::Lea!\n");



        
    DPRINTF(PointerDepGraph, "Lea Instruction Before Update: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()),
            inst->dyn_pid,
            inst->staticInst->getStaticPointerID());


    // read the data
    uint64_t  dataRegContent = inst->readDestReg(inst->staticInst.get(), 0); 
    
    // this is logically equal to a comparison
    TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 

    DPRINTF(PointerDepGraph, "updatePointerTrackerForLeaMicroop:: Checking Alias for dataRegContent[%s]=%x=%s\n", 
                            TheISA::IntRegIndexStr(dest), dataRegContent, _pid);

    // our assumption is that all the add instructions always transfer thier sources to thier 
    // destinations. Sometimes this is not true and the result is PID(0) although the sources are PID(n)
    // in this case we need to update the pointer tracker 
    if (_pid == TheISA::PointerID(0) && inst->dyn_pid != _pid)
    {
        DPRINTF(PointerDepGraph, "updatePointerTrackerForLeaMicroop:: Update is needed as Lea microop is not synced! inst->dyn_pid%s != %s\n", 
                            inst->dyn_pid, _pid);
        inst->dyn_pid = _pid;
        inst->staticInst->setStaticPointerID(_pid);  
        doUpdateForNoneLoadMicroops(inst);  
    }

    DPRINTF(PointerDepGraph, "Updated PID for Lea Instruction: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
                inst->seqNum,
                inst->pcState(), 
                inst->staticInst->disassemble(inst->pcState().instAddr()),
                inst->dyn_pid,
                inst->staticInst->getStaticPointerID()); 
            

}


// This function is called after doCommit, therefore we can't find the microop in the 
// pointer depdenecy graph
template <class Impl>
void
PointerDependencyGraph<Impl>::updatePointerTrackerForAPBaseCollectorMicroops(DynInstPtr &inst, ThreadContext* tc)
{
   
    DPRINTF(PointerDepGraph, "updatePointerTrackerForAPBaseCollectorMicroops::Allocation Point Microop Before Update: [%d][%s][%s][Dyn: %s][Static: %s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()),
            inst->dyn_pid,
            inst->staticInst->getStaticPointerID());
            

    DPRINTF(PointerDepGraph, "FetchArchRegsPids Reg Before AP Update:\n");
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        DPRINTF(PointerDepGraph, "FetchArchRegsPids[%s]  = %s\n", 
                TheISA::IntRegIndexStr(indx) , FetchArchRegsPid[indx]);
    }

    // for specific registers check whether they are synced or not
    // At this point, the collector in commit stage has added the new allocatoed object into
    // the capapbility cache
    for (size_t indx = 0; indx < X86ISA::NUM_INTREGS; indx++) {
        // read the data
        uint64_t  dataRegContent = cpu->readArchIntReg(indx, inst->threadNumber);
        // this is logically equal to a comparison with RAX reg
        TheISA::PointerID _pid = readPIDFromIntervalTree(dataRegContent, tc); 
        
        DPRINTF(PointerDepGraph, "REG[%s] = Value[%x] Current PID = %s Inst. PID = %s FetchArchRegsPid = %s\n", 
                TheISA::IntRegIndexStr(indx), dataRegContent, _pid, inst->dyn_pid, inst->FetchArchRegsPid[indx]);

        if (_pid == TheISA::PointerID(0))
        {
            assert(inst->FetchArchRegsPid[indx] == _pid && "inst->FetchArchRegsPid[indx] != _pid\n");
        }
        else 
        {
            //assert(_pid == inst->dyn_pid && "_pid != inst->dyn_inst!\n");
            if (indx == X86ISA::INTREG_RAX) 
                assert(inst->FetchArchRegsPid[indx] == _pid && "inst->FetchArchRegsPid[indx] != _pid\n");

            inst->FetchArchRegsPid[indx] = _pid;

        }

    }
    




    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        FetchArchRegsPid[indx] = inst->FetchArchRegsPid[indx];
        CommitArchRegsPid[indx] = inst->FetchArchRegsPid[indx];
    }

    DPRINTF(PointerDepGraph, "updatePointerTrackerForAPBaseCollectorMicroops::FetchArchRegsPids Reg After AP Update and Before Recursive Update:\n");
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        DPRINTF(PointerDepGraph, "FetchArchRegsPids[%s]  = %s\n", 
                TheISA::IntRegIndexStr(indx) , FetchArchRegsPid[indx]);
    }

    std::map<uint64_t, uint64_t> MicroopsToUpdate;
    // order the microops that are tracked after this pointer refill
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].cbegin(); it != dependGraph[indx].cend(); it++)
        {
            if (it->inst->seqNum > inst->seqNum) {
                MicroopsToUpdate.insert(std::make_pair(it->inst->seqNum, indx));
            }
        }
    }

    // now one by one find all the instruction with seqNum greater
    // than load uop and update their uops in order!
    for (auto& microops : MicroopsToUpdate)
    {
        uint64_t SeqNumber = microops.first;
        uint64_t Indx = microops.second;
        for (auto it = dependGraph[Indx].begin(); it != dependGraph[Indx].end(); it++)
        {
            if (it->inst->seqNum == SeqNumber) {
                InternalUpdate(it->inst, false);
                it->pid = it->inst->dyn_pid;
                break;
            }
        }
    }



    DPRINTF(PointerDepGraph, "updatePointerTrackerForAPBaseCollectorMicroops::Dependency Graph After Updating:\n");
    dump();

    // assert(0);
}

#endif // __CPU_O3_DEP_GRAPH_HH__
