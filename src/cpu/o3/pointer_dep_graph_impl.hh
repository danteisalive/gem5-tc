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
PointerDependencyGraph<Impl>::~PointerDependencyGraph()
{}


template <class Impl>
void
PointerDependencyGraph<Impl>::reset()
{

    for (int i = 0; i < TheISA::NumIntRegs; ++i) {
        dependGraph[i].clear();
        FetchArchRegsPid[i] = TheISA::PointerID(0);
        CommitArchRegsPid[i] = TheISA::PointerID(0);
    }
}

template <class Impl>
void
PointerDependencyGraph<Impl>::insert(DynInstPtr &inst)
{
    InternalUpdate(inst, true);   
}

template <class Impl>
void
PointerDependencyGraph<Impl>::doSquash(uint64_t squashedSeqNum){


    DPRINTF(PointerDepGraph, "Squashing Alias Until Sequence Number: [%d]\n", squashedSeqNum);
    DPRINTF(PointerDepGraph, "Dependency Graph Before Squashing:\n");
    dump();

    // if the producer seqNum is greater than squashedSeqNum then
    // remove it and all of the consumers as they are all will be squashed
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {

        // Erase all even numbers (C++11 and later)
        for (auto it = dependGraph[i].begin(); it != dependGraph[i].end(); )
        {
            if (it->inst->seqNum > squashedSeqNum) {
                it->inst = NULL;
                it = dependGraph[i].erase(it);
            } else {
                ++it;
            }
        }

    } // for loop

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
    for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
      if (inst->staticInst->destRegIdx(i).isIntReg() &&
          (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
      {
            // the inst should be at the end of the queue
            int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
            
            panic_if(dependGraph[dest_reg_idx].back().inst->seqNum !=
                    inst->seqNum,
                    "Dangling inst in PointerDependGraph");

            // before commiting, perform a sanity check
            PerformSanityCheck(inst);

            DPRINTF(PointerDepGraph, "Setting: CommitArchRegsPid[%d]=%s\n",
                    TheISA::IntRegIndexStr(dest_reg_idx), dependGraph[dest_reg_idx].back().pid);
            CommitArchRegsPid[dest_reg_idx] =
                            dependGraph[dest_reg_idx].back().pid;

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
        inst->isCallocBaseCollectorMicroop() ||
        inst->isReallocBaseCollectorMicroop()))
    {
        // what should we do here?
    }
    else if ((inst->isFreeCallMicroop() || 
              inst->isReallocSizeCollectorMicroop()))
    {
        // what should we do here?
    }
    else if (inst->staticInst->getName() == "mov")  {TransferMovMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "st")   {TransferStoreMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "stis")   {TransferStoreInStackMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "ld")   {TransferLoadMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "ldis") {TransferLoadInStackMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "add") {TransferAddMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "sub") {TransferSubMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "addi") {TransferAddImmMicroops(inst, false, true);}
    else if (inst->staticInst->getName() == "subi") {TransferSubImmMicroops(inst, false, true);}
    
    else {

        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
            if (inst->destRegIdx(i).isIntReg())
            {
                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
                uint16_t dest_reg_idx = x86_inst->getUnflattenRegIndex(inst->staticInst->destRegIdx(i)); 
                dest_reg_idx = dest_reg_idx;
                // what do need to check here?! 
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
          DPRINTF(PointerDepGraph, "[%s] ==> [%d][%s][%s] %d\n", 
                                TheISA::IntRegIndexStr(i),
                                it->inst->seqNum,
                                it->inst->pcState(), 
                                it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                                it->inst->dyn_pid);
     
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
void
PointerDependencyGraph<Impl>::doUpdate(DynInstPtr& inst)
{
    

    if (inst->isBoundsCheckMicroop()) return; // we do not save these
    assert (inst->isLoad() && "doUpdate called with a non-load instruction!") ;
    assert((inst->staticInst->getName() == "ld" || inst->staticInst->getName() == "ldis") && 
            "Not a ld or ldis instruction called with doUpdate()\n");
    assert(inst->staticInst->getDataSize() == 8 && "data size is not 8!");

    DPRINTF(PointerDepGraph, "IEW Updating Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(PointerDepGraph, "Dependency Graph Before IEW Updating:\n");
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
                assert(dest < TheISA::NumIntRegs);
                inst->FetchArchRegsPid[dest] = _pid;
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
    // now one by one find all the instruction with seqNum greater
    // than load uop and update their uops
     for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
         // Erase  (C++11 and later)
         for (auto it = dependGraph[indx].rbegin(); it != dependGraph[indx].rend(); it++)
         {
             if (it->inst->seqNum > inst->seqNum) {
               InternalUpdate(it->inst, false);
             }
         }
     }



    DPRINTF(PointerDepGraph, "Dependency Graph After IEW Updating:\n");
    dump();



}

template <class Impl>
void
PointerDependencyGraph<Impl>::InternalUpdate(DynInstPtr &inst, bool track)
{

    if (inst->isBoundsCheckMicroop()) return;

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
        inst->isCallocBaseCollectorMicroop() ||
        inst->isReallocBaseCollectorMicroop()))
    {
        // here we generate a new PID and insert it into rax
        dependGraph[X86ISA::INTREG_RAX].push_front(
                                        PointerDepEntry(inst, inst->dyn_pid));
        FetchArchRegsPid[X86ISA::INTREG_RAX] = inst->dyn_pid;

        DPRINTF(PointerDepGraph, "Malloc/Calloc/Realloc base collector is called! Assigned PID=%s\n", 
                FetchArchRegsPid[X86ISA::INTREG_RAX]);
    }
    else if ((track) && 
            (inst->isFreeCallMicroop() || 
            inst->isReallocSizeCollectorMicroop()))
    {

        dependGraph[X86ISA::INTREG_RDI].push_front(
                                        PointerDepEntry(inst, inst->dyn_pid));
        //FetchArchRegsPid[X86ISA::INTREG_RDI] = inst->dyn_pid;
        DPRINTF(PointerDepGraph, "Free/Realloc is called! Invalidating PID=%s\n", 
                FetchArchRegsPid[X86ISA::INTREG_RDI]);
    }
    else if (inst->staticInst->getName() == "mov")  {TransferMovMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "st")   {TransferStoreMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "stis")   {TransferStoreInStackMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "ld")   {TransferLoadMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "ldis") {TransferLoadInStackMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "add") {TransferAddMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "sub") {TransferSubMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "addi") {TransferAddImmMicroops(inst, track, false);}
    else if (inst->staticInst->getName() == "subi") {TransferSubImmMicroops(inst, track, false);}
    else {

        TheISA::PointerID _pid{0} ;
        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
          if (inst->staticInst->destRegIdx(i).isIntReg() &&
              (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
          {
             // assign pid to all of the dest regs
             int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
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
                if ((dataSize == 4 || dataSize == 2 || dataSize == 1) && CommitArchRegsPid[src1] != TheISA::PointerID(0))
                {
                    assert(false && "Found a 1/2/4 bytes Mov Inst with non-zero PID!\n");
                }
                return;
            }


            DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== FetchArchRegsPid[%s]=[%d]\n", 
                TheISA::IntRegIndexStr(dest),
                TheISA::IntRegIndexStr(src1), 
                FetchArchRegsPid[src1].getPID());


            TheISA::PointerID _pid = FetchArchRegsPid[src1];

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
                FetchArchRegsPid[src2].getPID());
    
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
                FetchArchRegsPid[src2].getPID());
    
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
           panic_if(CommitArchRegsPid[src0] != TheISA::PointerID(0), 
                        "TransferLoadeMicroops :: Found a 1/2 Ld Microop with non-zero PID Index!\n");
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
                    FetchArchRegsPid[dest].getPID());

        
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
                    FetchArchRegsPid[dest].getPID());


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
                    FetchArchRegsPid[dest].getPID());

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
                    FetchArchRegsPid[dest].getPID());


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
        panic_if((dataSize == 4 || dataSize == 2 || dataSize == 1) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) || CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "Found a 1/2/4 bytes Sub Inst with non-zero PID sources! " 
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        panic_if((dataSize == 8) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && CommitArchRegsPid[src0] != TheISA::PointerID(0)), 
                "TransferStoreMicroops :: Found a Sub inst with both regs non-zero PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);
        // dest = src0(PID(0)) - src1(PID(n))
        panic_if((dataSize == 8) && 
                (CommitArchRegsPid[src1] != TheISA::PointerID(0) && CommitArchRegsPid[src0] == TheISA::PointerID(0)), 
                "TransferStoreMicroops :: Found a Sub inst with src1 reg non-zero PID but src0 reg zeo PID! "
                "SRC1 = %s SRC2 = %s\n", CommitArchRegsPid[src0], CommitArchRegsPid[src1]);

        return;
    }


    DPRINTF(PointerDepGraph, "\t\tFetchArchRegsPid[%s] <=== SRC1Pid[%d][%s] - SRC2Pid[%d][%s]\n", 
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

#endif // __CPU_O3_DEP_GRAPH_HH__
