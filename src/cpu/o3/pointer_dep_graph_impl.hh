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

    if (inst->isBoundsCheckMicroop()) return;


    DPRINTF(TypeTracker, "Tracking Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));

    DPRINTF(TypeTracker, "Dependency Graph Before Insert:\n");
    dump();

    //transfer capabilities
    if (inst->isMallocBaseCollectorMicroop() ||
        inst->isCallocBaseCollectorMicroop() ||
        inst->isReallocBaseCollectorMicroop())
    {
        // here we generate a new PID and insert it into rax
        dependGraph[X86ISA::INTREG_RAX].push_front(
                                        PointerDepEntry(inst, inst->dyn_pid));
        FetchArchRegsPid[X86ISA::INTREG_RAX] = inst->dyn_pid;

        dump(inst);
    }
    else if (inst->isFreeCallMicroop() ||
            inst->isReallocSizeCollectorMicroop())
    {
        dependGraph[X86ISA::INTREG_RDI].push_front(
                                        PointerDepEntry(inst, inst->dyn_pid));
        FetchArchRegsPid[X86ISA::INTREG_RDI] = inst->dyn_pid;
        dump(inst);
    }


    else if (inst->staticInst->getName() == "mov") {TransferMovMicroops(inst);}
    
    // this can be a potential pointer refill
    // else if (inst->isLoad() && inst->staticInst->getDataSize() == 8)
    // {
    //     // get the predicted PID for this load
    //     TheISA::PointerID _pid = inst->macroop->getMacroopPid();
    //     // insert an entry for the destination reg
    //     for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
    //         if (inst->staticInst->destRegIdx(i).isIntReg() &&
    //             (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
    //         {
    //             int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
    //             dependGraph[dest_reg_idx].push_front(
    //                                           PointerDepEntry(inst, _pid));
    //             FetchArchRegsPid[dest_reg_idx] = _pid;
    //         }
    //     }
    // }
    // else if (inst->isLoad() && inst->staticInst->getDataSize() != 8)
    // {
    //     // this is defenitly not a pointer refill
    //     // set the dest regs as PID(0)
    //     TheISA::PointerID _pid = TheISA::PointerID(0);
    //     for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
    //         if (inst->staticInst->destRegIdx(i).isIntReg() &&
    //            (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
    //         {
    //             int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
    //             dependGraph[dest_reg_idx].push_front(
    //                                           PointerDepEntry(inst, _pid));
    //             FetchArchRegsPid[dest_reg_idx] = _pid;
    //         }
    //     }
    // }
    else {

        TheISA::PointerID _pid{0} ;
        // for (size_t i = 0; i < inst->staticInst->numSrcRegs(); i++) {
        //   if (inst->staticInst->srcRegIdx(i).isIntReg() &&
        //       (inst->staticInst->srcRegIdx(i).index() < TheISA::NumIntRegs))
        //   {
        //       // if one of the sources is not pid(0), assign it to pid
        //       // and break;
        //       int src_reg_idx = inst->staticInst->srcRegIdx(i).index();
        //       if (FetchArchRegsPid[src_reg_idx] != TheISA::PointerID(0))
        //       {
        //         _pid = FetchArchRegsPid[src_reg_idx];
        //         break;
        //       }
        //   }
        // }

        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
          if (inst->staticInst->destRegIdx(i).isIntReg() &&
              (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
          {
             // assign pid to all of the dest regs
             int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
             dependGraph[dest_reg_idx].push_front(
                                        PointerDepEntry(inst, _pid));
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

    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {
        //snapshot
        inst->FetchArchRegsPid[i] = FetchArchRegsPid[i];
    }

    DPRINTF(TypeTracker, "Dependency Graph After Insert:\n");
    dump();

}

template <class Impl>
void
PointerDependencyGraph<Impl>::doSquash(uint64_t squashedSeqNum){


    DPRINTF(TypeTracker, "Squashing Alias Until Sequence Number: [%d]\n", squashedSeqNum);
    DPRINTF(TypeTracker, "Dependency Graph Before Squashing:\n");
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

    DPRINTF(TypeTracker, "Dependency Graph After Squashing:\n");
    dump();
}

template <class Impl>
void
PointerDependencyGraph<Impl>::doCommit(DynInstPtr &inst){

    if (inst->isBoundsCheckMicroop()) return; // we do not save these


    DPRINTF(TypeTracker, "Commiting Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(TypeTracker, "Dependency Graph Before Commiting:\n");
    dump();

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
         CommitArchRegsPid[dest_reg_idx] =
                          dependGraph[dest_reg_idx].back().pid;
         dependGraph[dest_reg_idx].back().inst = NULL;
         dependGraph[dest_reg_idx].pop_back();

      }
    }

    DPRINTF(TypeTracker, "Dependency Graph After Commiting:\n");
    dump();
}

template <class Impl>
void
PointerDependencyGraph<Impl>::dump(DynInstPtr &inst)
{

    DPRINTF(TypeTracker, "---------------------------------------------------------------------\n");
    for (size_t i = 0; i < TheISA::NumIntRegs; i++)
    {
        if (FetchArchRegsPid[i] != TheISA::PointerID(0) && CommitArchRegsPid[i] != TheISA::PointerID(0))
            DPRINTF(TypeTracker, "FetchArchRegsPid[%s]\t=\t[%d]\t\tCommitArchRegsPid[%s]\t=\t[%d]\n", 
                    TheISA::IntRegIndexStr(i), FetchArchRegsPid[i], TheISA::IntRegIndexStr(i), CommitArchRegsPid[i]);
    }
    

    DPRINTF(TypeTracker, "---------------------------------------------------------------------\n");

}

template <class Impl>
void
PointerDependencyGraph<Impl>::dump()
{
    DPRINTF(TypeTracker, "---------------------------------------------------------------------\n");
    for (size_t i = 0; i < TheISA::NumIntRegs; i++) {

        for (auto it = dependGraph[i].begin(); it != dependGraph[i].end(); it++)
        {
          assert(it->inst); // shouldnt be a null inst
          DPRINTF(TypeTracker, "[%s] ==> [%d][%s][%s] %d\n", 
                                TheISA::IntRegIndexStr(i),
                                it->inst->seqNum,
                                it->inst->pcState(), 
                                it->inst->staticInst->disassemble(it->inst->pcState().instAddr()),
                                it->pid);
     
        }
    }
    DPRINTF(TypeTracker, "---------------------------------------------------------------------\n");

}

template <class Impl>
void
PointerDependencyGraph<Impl>::doUpdate(DynInstPtr& inst)
{
    

    if (inst->isBoundsCheckMicroop()) return; // we do not save these
    if (!inst->isLoad()) {
        panic("doUpdate called with a non-load instruction!");
    }

    DPRINTF(TypeTracker, "IEW Updating Alias for Instruction: [%d][%s][%s]\n", 
            inst->seqNum,
            inst->pcState(), 
            inst->staticInst->disassemble(inst->pcState().instAddr()));
    DPRINTF(TypeTracker, "Dependency Graph Before IEW Updating:\n");
    dump();


    //find the load uop
    // we should defenitly find it othersie it's a panic!
    // doUpdate happends before squash so we should find it in the DEP Graph
    bool found = false;
    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        // Erase  (C++11 and later)
        for (auto it = dependGraph[indx].begin();
            it != dependGraph[indx].end(); it++)
        {
            //if found: update and return
            if (it->inst->seqNum == inst->seqNum) {
                // get the actual PID for this load
                TheISA::PointerID _pid = inst->macroop->getMacroopPid();
                // insert an entry for the destination reg
                for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
                    if (inst->staticInst->destRegIdx(i).isIntReg() &&
                        (inst->staticInst->destRegIdx(i).index() <
                                                  TheISA::NumIntRegs))
                    {
                        int dest_reg_idx =
                            inst->staticInst->destRegIdx(i).index();
                        panic_if(dest_reg_idx != indx,
                          "destination reg id does not \
                           match with updated load!");
                        it->pid = _pid;
                        inst->FetchArchRegsPid[dest_reg_idx] = _pid;
                    }
                }

                found = true;
                break;

            }
        }

    } // for loop
    if (!found) {
        return; // in the process of squashing.
    }
    /*panic_if(!found, "Could not find the load uop\
                     when updating the DEP Graph!");*/

    for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
        FetchArchRegsPid[indx] = inst->FetchArchRegsPid[indx];
    }
    // now one by one find all the instruction with seqNum greater
    // than load uop and update their uops
     for (size_t indx = 0; indx < TheISA::NumIntRegs; indx++) {
         // Erase  (C++11 and later)
         for (auto it = dependGraph[indx].rbegin();
           it != dependGraph[indx].rend(); it++)
         {
             if (it->inst->seqNum > inst->seqNum) {
               InternalUpdate(it->inst);
               for (size_t idx = 0; idx < TheISA::NumIntRegs; idx++) {
                   it->inst->FetchArchRegsPid[idx] = FetchArchRegsPid[idx];
               }
             }
         }
     }

    // now update FetchArchRegsPid with the latest values
    //for (size_t i = 0; i < TheISA::NumIntRegs; i++) {
    //    FetchArchRegsPid[i] = dependGraph[i].front().pid;
    //}


    DPRINTF(TypeTracker, "Dependency Graph After IEW Updating:\n");
    dump();



}

template <class Impl>
void
PointerDependencyGraph<Impl>::InternalUpdate(DynInstPtr &inst)
{
    assert(0);
    #define ENABLE_DEP_GRAPH_INTERNAL_UPDATE 0

    if (inst->isBoundsCheckMicroop()) return;

    if (inst->isLoad() && inst->staticInst->getDataSize() == 8)
    {
        // get the predicted PID for this load
        TheISA::PointerID _pid = inst->macroop->getMacroopPid();
        // insert an entry for the destination reg
        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
            if (inst->staticInst->destRegIdx(i).isIntReg() &&
                (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
            {
                int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
                //dependGraph[dest_reg_idx].push_front(
                  //                            PointerDepEntry(inst, _pid));
                FetchArchRegsPid[dest_reg_idx] = _pid;
            }
        }
    }
    else if (inst->isLoad() && inst->staticInst->getDataSize() != 8)
    {
        // this is defenitly not a pointer refill
        // set the dest regs as PID(0)
        TheISA::PointerID _pid = TheISA::PointerID(0);
        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
            if (inst->staticInst->destRegIdx(i).isIntReg() &&
               (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
            {
                int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
                //dependGraph[dest_reg_idx].push_front(
                  //                            PointerDepEntry(inst, _pid));
                FetchArchRegsPid[dest_reg_idx] = _pid;
            }
        }
    }
    else {

        TheISA::PointerID _pid{0} ;
        for (size_t i = 0; i < inst->staticInst->numSrcRegs(); i++) {
          if (inst->staticInst->srcRegIdx(i).isIntReg() &&
              (inst->staticInst->srcRegIdx(i).index() < TheISA::NumIntRegs))
          {
              // if one of the sources is not pid(0), assign it to pid
              // and break;
              int src_reg_idx = inst->staticInst->srcRegIdx(i).index();
              if (FetchArchRegsPid[src_reg_idx] != TheISA::PointerID(0))
              {
                _pid = FetchArchRegsPid[src_reg_idx];
                break;
              }
          }
        }

        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
          if (inst->staticInst->destRegIdx(i).isIntReg() &&
              (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
          {
             // assign pid to all of the dest regs
             int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
             //dependGraph[dest_reg_idx].push_front(
               //                         PointerDepEntry(inst, _pid));
             FetchArchRegsPid[dest_reg_idx] = _pid;
          }
        }

    }
    // defenitly this is not a pointer transefer
    // zero out all the dest regs
/*    else if (inst->staticInst->getDataSize() != 8) {

        TheISA::PointerID _pid = TheISA::PointerID(0);
        for (size_t i = 0; i < inst->staticInst->numDestRegs(); i++) {
          if (inst->staticInst->destRegIdx(i).isIntReg() &&
              (inst->staticInst->destRegIdx(i).index() < TheISA::NumIntRegs))
          {
              // zero out all dest regs
              int dest_reg_idx = inst->staticInst->destRegIdx(i).index();
              //dependGraph[dest_reg_idx].push_front(
                //                        PointerDepEntry(inst, _pid));
              FetchArchRegsPid[dest_reg_idx] = _pid;
          }
        }
    }
*/
    // zero out all interface regs for the next macroopp
    if (inst->isLastMicroop()){
      TheISA::PointerID _pid = TheISA::PointerID(0);
      for (size_t i = X86ISA::NUM_INTREGS; i < TheISA::NumIntRegs; i++) {
          //zero out all dest regs
          FetchArchRegsPid[i] = _pid;
      }
      //dump();
    }



      if (ENABLE_DEP_GRAPH_INTERNAL_UPDATE)
      {
        dump();
        std::cout << "-------------END--------------\n";
      }


}


template <class Impl>
void
PointerDependencyGraph<Impl>::TransferMovMicroops(DynInstPtr &inst)
{

    assert(inst->numDestRegs() == 1 && "Invalid number of dest regs!\n");
    TheISA::RegOp * inst_regop = (TheISA::RegOp * )inst->staticInst.get(); 
    const uint8_t dataSize = inst_regop->dataSize;
    assert(dataSize == 8 || dataSize == 4 || dataSize == 2 || dataSize == 1);

    X86ISAInst::Mov * inst_mov = dynamic_cast<X86ISAInst::Mov*>(inst->staticInst.get()); 
    X86ISAInst::MovFlags * inst_mov_flags = dynamic_cast<X86ISAInst::MovFlags*>(inst->staticInst.get()); 
    if (inst_mov != nullptr || inst_mov_flags != nullptr)
    {

            X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();

            uint16_t src0 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(0)); 
            uint16_t src1 = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(1));
            uint16_t dest = x86_inst->getUnflattenRegIndex(inst->srcRegIdx(2));
            assert(src0 < TheISA::NumIntRegs && src1 < TheISA::NumIntRegs); 
            assert(dest < TheISA::NumIntRegs);



            DPRINTF(TypeTracker, "\t\tFetchArchRegsPid[%s] <=== FetchArchRegsPid[%s]=[%d]\n", 
                TheISA::IntRegIndexStr(dest),
                TheISA::IntRegIndexStr(src1), 
                FetchArchRegsPid[src1].getPID());

            if ((dataSize == 4 || dataSize == 2 || dataSize == 1) && FetchArchRegsPid[src0] != TheISA::PointerID(0))
            {
                assert(false && "Found a 1/2/4 bytes Mov Inst with non-zero PID!\n");
            }

            TheISA::PointerID _pid = FetchArchRegsPid[src0];

            FetchArchRegsPid[dest] = _pid;

            dependGraph[dest].push_front(PointerDepEntry(inst, _pid));
            
    }
    else 
    {
        assert(false && "Found a mov inst that is not Mov or MovFlags!\n");
    }
}

#endif // __CPU_O3_DEP_GRAPH_HH__
