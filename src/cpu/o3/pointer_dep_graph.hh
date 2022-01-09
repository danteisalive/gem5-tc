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

#ifndef __CPU_O3_POINTER_DEP_GRAPH_HH__
#define __CPU_O3_POINTER_DEP_GRAPH_HH__

#include "arch/x86/generated/decoder.hh"
#include "cpu/o3/comm.hh"
#include "debug/TypeTracker.hh"

#include "config/the_isa.hh"

/** Node in a linked list. */
template <class Impl>
class PointerDependencyEntry
{
  public:

    typedef typename Impl::DynInstPtr DynInstPtr;

    PointerDependencyEntry()
        : inst(NULL), pid(TheISA::PointerID(0))
    { }

    PointerDependencyEntry(DynInstPtr _inst, TheISA::PointerID _pid)
        : inst(_inst), pid(_pid)
    { }
    DynInstPtr inst;
    TheISA::PointerID pid{0};
};

/** Array of linked list that maintains the dependencies between
 * producing instructions and consuming instructions.  Each linked
 * list represents a single physical register, having the future
 * producer of the register's value, and all consumers waiting on that
 * value on the list.  The head node of each linked list represents
 * the producing instruction of that register.  Instructions are put
 * on the list upon reaching the IQ, and are removed from the list
 * either when the producer completes, or the instruction is squashed.
*/
template <class Impl>
class PointerDependencyGraph
{
  public:
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef PointerDependencyEntry<Impl> PointerDepEntry;

    /** Default construction.  Must call resize() prior to use. */
    PointerDependencyGraph()
        : numEntries(0), memAllocCounter(0), nodesTraversed(0), nodesRemoved(0)
    {
        for (int i = 0; i < TheISA::NumIntRegs; ++i) {
            dependGraph[i].clear();
            FetchArchRegsPid[i] = TheISA::PointerID(0);
            CommitArchRegsPid[i] = TheISA::PointerID(0);
        }
    }

    ~PointerDependencyGraph();

    /** Resize the dependency graph to have num_entries registers. */
    // void resize(int num_entries);

    /** Clears all of the linked lists. */
    void reset();

    /** Inserts an instruction to be dependent on the given index. */
    void insert(DynInstPtr &new_inst);


    void doSquash(uint64_t squashedSeqNum);

    void doUpdate(DynInstPtr& inst);
    void InternalUpdate(DynInstPtr &inst);
    /** Removes an instruction from a single linked list. */
    void doCommit(DynInstPtr &inst);

    /** Debugging function to dump out the dependency graph.
     */
    void dump(DynInstPtr &inst);
    void dump();



  private:
    /** Array of linked lists.  Each linked list is a list of all the
     *  instructions that depend upon a given register.  The actual
     *  register's index is used to index into the graph; ie all
     *  instructions in flight that are dependent upon r34 will be
     *  in the linked list of dependGraph[34].
     */

    std::array<std::deque<PointerDepEntry>, TheISA::NumIntRegs> dependGraph;
    std::array<TheISA::PointerID, TheISA::NumIntRegs> CommitArchRegsPid;
    std::array<TheISA::PointerID, TheISA::NumIntRegs> FetchArchRegsPid;

    /** Number of linked lists; identical to the number of registers. */
    int numEntries;

    // Debug variable, remove when done testing.
    unsigned memAllocCounter;

    void TransferMovMicroops(DynInstPtr &inst);
    void TransferAddMicroops(DynInstPtr &inst);

  public:
    // Debug variable, remove when done testing.
    uint64_t nodesTraversed;
    // Debug variable, remove when done testing.
    uint64_t nodesRemoved;

    std::array<TheISA::PointerID, TheISA::NumIntRegs>
     getFetchArchRegsPidArray(){
       return FetchArchRegsPid;
    }

    void setFetchArchRegsPidArray(int idx, TheISA::PointerID _pid){
        FetchArchRegsPid[idx] = _pid;
    }
    void setCommitArchRegsPidArray(int idx, TheISA::PointerID _pid){
        CommitArchRegsPid[idx] = _pid;
    }
};


#endif // __CPU_O3_DEP_GRAPH_HH__
