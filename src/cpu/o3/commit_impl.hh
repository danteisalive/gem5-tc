/*
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2010-2014, 2017 ARM Limited
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
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
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
 *          Korey Sewell
 */
#ifndef __CPU_O3_COMMIT_IMPL_HH__
#define __CPU_O3_COMMIT_IMPL_HH__

#include <algorithm>
#include <set>
#include <map>
#include <string>
#include <tuple>
#include "arch/utility.hh"
#include "base/loader/symtab.hh"
#include "base/cp_annotate.hh"
#include "config/the_isa.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/o3/commit.hh"
#include "cpu/o3/thread_state.hh"
#include "cpu/base.hh"
#include "cpu/exetrace.hh"
#include "cpu/timebuf.hh"
#include "debug/Activity.hh"
#include "debug/Commit.hh"
#include "debug/CommitRate.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/O3PipeView.hh"
#include "params/DerivO3CPU.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"
#include "debug/Capability.hh"
#include "debug/TypeTracker.hh"
#include "debug/Allocator.hh"
#include "debug/AliasCache.hh"
#include "debug/PointerDepGraph.hh"
#include "debug/TrackFunctionObject.hh"

using namespace std;

template <class Impl>
void
DefaultCommit<Impl>::processTrapEvent(ThreadID tid)
{
    // This will get reset by commit if it was switched out at the
    // time of this event processing.
    trapSquash[tid] = true;
}

template <class Impl>
DefaultCommit<Impl>::DefaultCommit(O3CPU *_cpu, DerivO3CPUParams *params)
    : cpu(_cpu),
      iewToCommitDelay(params->iewToCommitDelay),
      commitToIEWDelay(params->commitToIEWDelay),
      renameToROBDelay(params->renameToROBDelay),
      fetchToCommitDelay(params->commitToFetchDelay),
      renameWidth(params->renameWidth),
      commitWidth(params->commitWidth),
      numThreads(params->numThreads),
      drainPending(false),
      drainImminent(false),
      trapLatency(params->trapLatency),
      canHandleInterrupts(true),
      avoidQuiesceLiveLock(false)
{
    if (commitWidth > Impl::MaxWidth)
        fatal("commitWidth (%d) is larger than compiled limit (%d),\n"
             "\tincrease MaxWidth in src/cpu/o3/impl.hh\n",
             commitWidth, static_cast<int>(Impl::MaxWidth));

    _status = Active;
    prevRSPValue = 0;
    NumOfAllocations=0;
    _nextStatus = Inactive;
    std::string policy = params->smtCommitPolicy;

    //Convert string to lowercase
    std::transform(policy.begin(), policy.end(), policy.begin(),
                   (int(*)(int)) tolower);

    //Assign commit policy
    if (policy == "aggressive"){
        commitPolicy = Aggressive;

        DPRINTF(Commit,"Commit Policy set to Aggressive.\n");
    } else if (policy == "roundrobin"){
        commitPolicy = RoundRobin;

        //Set-Up Priority List
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            priority_list.push_back(tid);
        }

        DPRINTF(Commit,"Commit Policy set to Round Robin.\n");
    } else if (policy == "oldestready"){
        commitPolicy = OldestReady;

        DPRINTF(Commit,"Commit Policy set to Oldest Ready.");
    } else {
        assert(0 && "Invalid SMT Commit Policy. Options Are: {Aggressive,"
               "RoundRobin,OldestReady}");
    }

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        commitStatus[tid] = Idle;
        changedROBNumEntries[tid] = false;
        checkEmptyROB[tid] = false;
        trapInFlight[tid] = false;
        committedStores[tid] = false;
        trapSquash[tid] = false;
        tcSquash[tid] = false;
        pc[tid].set(0);
        lastCommitedSeqNum[tid] = 0;
        squashAfterInst[tid] = NULL;
    }
    interrupt = NoFault;

    std::ofstream tyCHEExecutionSanityCheck;
    if (params->enable_capability)
        tyCHEExecutionSanityCheck.open("./m5out/ExecSanityTyche.tyche", std::ios_base::out);
    else 
        tyCHEExecutionSanityCheck.open("./m5out/ExecSanityRaw.tyche", std::ios_base::out);
    tyCHEExecutionSanityCheck.close();

}

template <class Impl>
std::string
DefaultCommit<Impl>::name() const
{
    return cpu->name() + ".commit";
}

template <class Impl>
void
DefaultCommit<Impl>::regProbePoints()
{
    ppCommit = new ProbePointArg<DynInstPtr>(cpu->getProbeManager(), "Commit");
    ppCommitStall =
          new ProbePointArg<DynInstPtr>(cpu->getProbeManager(), "CommitStall");
    ppSquash = new ProbePointArg<DynInstPtr>(cpu->getProbeManager(), "Squash");
}

template <class Impl>
void
DefaultCommit<Impl>::regStats()
{
    using namespace Stats;
    commitSquashedInsts
        .name(name() + ".commitSquashedInsts")
        .desc("The number of squashed insts skipped by commit")
        .prereq(commitSquashedInsts);

    commitSquashedInstsDueToMissPID
        .name(name() + ".commitSquashedInstsDueToMissPID")
        .desc("The number of squashed insts skipped by commit")
        .prereq(commitSquashedInstsDueToMissPID);

    commitNonSpecStalls
        .name(name() + ".commitNonSpecStalls")
        .desc("The number of times commit has been forced to stall to "
              "communicate backwards")
        .prereq(commitNonSpecStalls);

    branchMispredicts
        .name(name() + ".branchMispredicts")
        .desc("The number of times a branch was mispredicted")
        .prereq(branchMispredicts);

    numCommittedDist
        .init(0,commitWidth,1)
        .name(name() + ".committed_per_cycle")
        .desc("Number of insts commited each cycle")
        .flags(Stats::pdf)
        ;

    instsCommitted
        .init(cpu->numThreads)
        .name(name() + ".committedInsts")
        .desc("Number of instructions committed")
        .flags(total)
        ;

    opsCommitted
        .init(cpu->numThreads)
        .name(name() + ".committedOps")
        .desc("Number of ops (including micro ops) committed")
        .flags(total)
        ;

    statComSwp
        .init(cpu->numThreads)
        .name(name() + ".swp_count")
        .desc("Number of s/w prefetches committed")
        .flags(total)
        ;

    statComRefs
        .init(cpu->numThreads)
        .name(name() +  ".refs")
        .desc("Number of memory references committed")
        .flags(total)
        ;

    statComLoads
        .init(cpu->numThreads)
        .name(name() +  ".loads")
        .desc("Number of loads committed")
        .flags(total)
        ;

    statComMembars
        .init(cpu->numThreads)
        .name(name() +  ".membars")
        .desc("Number of memory barriers committed")
        .flags(total)
        ;

    statComBranches
        .init(cpu->numThreads)
        .name(name() + ".branches")
        .desc("Number of branches committed")
        .flags(total)
        ;

    statComFloating
        .init(cpu->numThreads)
        .name(name() + ".fp_insts")
        .desc("Number of committed floating point instructions.")
        .flags(total)
        ;

    statComVector
        .init(cpu->numThreads)
        .name(name() + ".vec_insts")
        .desc("Number of committed Vector instructions.")
        .flags(total)
        ;

    statComInteger
        .init(cpu->numThreads)
        .name(name()+".int_insts")
        .desc("Number of committed integer instructions.")
        .flags(total)
        ;

    statComFunctionCalls
        .init(cpu->numThreads)
        .name(name()+".function_calls")
        .desc("Number of function calls committed.")
        .flags(total)
        ;

    statCommittedInstType
        .init(numThreads,Enums::Num_OpClass)
        .name(name() + ".op_class")
        .desc("Class of committed instruction")
        .flags(total | pdf | dist)
        ;
    statCommittedInstType.ysubnames(Enums::OpClassStrings);

    commitEligibleSamples
        .name(name() + ".bw_lim_events")
        .desc("number cycles where commit BW limit reached")
        ;
}

template <class Impl>
void
DefaultCommit<Impl>::setThreads(std::vector<Thread *> &threads)
{
    thread = threads;
}

template <class Impl>
void
DefaultCommit<Impl>::setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr)
{
    timeBuffer = tb_ptr;

    // Setup wire to send information back to IEW.
    toIEW = timeBuffer->getWire(0);

    // Setup wire to read data from IEW (for the ROB).
    robInfoFromIEW = timeBuffer->getWire(-iewToCommitDelay);
}

template <class Impl>
void
DefaultCommit<Impl>::setFetchQueue(TimeBuffer<FetchStruct> *fq_ptr)
{
    fetchQueue = fq_ptr;

    // Setup wire to get instructions from rename (for the ROB).
    fromFetch = fetchQueue->getWire(-fetchToCommitDelay);
}

template <class Impl>
void
DefaultCommit<Impl>::setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr)
{
    renameQueue = rq_ptr;

    // Setup wire to get instructions from rename (for the ROB).
    fromRename = renameQueue->getWire(-renameToROBDelay);
}

template <class Impl>
void
DefaultCommit<Impl>::setIEWQueue(TimeBuffer<IEWStruct> *iq_ptr)
{
    iewQueue = iq_ptr;

    // Setup wire to get instructions from IEW.
    fromIEW = iewQueue->getWire(-iewToCommitDelay);
}

template <class Impl>
void
DefaultCommit<Impl>::setIEWStage(IEW *iew_stage)
{
    iewStage = iew_stage;
}

template<class Impl>
void
DefaultCommit<Impl>::setActiveThreads(list<ThreadID> *at_ptr)
{
    activeThreads = at_ptr;
}

template <class Impl>
void
DefaultCommit<Impl>::setRenameMap(RenameMap rm_ptr[])
{
    for (ThreadID tid = 0; tid < numThreads; tid++)
        renameMap[tid] = &rm_ptr[tid];
}

template <class Impl>
void
DefaultCommit<Impl>::setROB(ROB *rob_ptr)
{
    rob = rob_ptr;
}

template <class Impl>
void
DefaultCommit<Impl>::startupStage()
{
    rob->setActiveThreads(activeThreads);
    rob->resetEntries();

    // Broadcast the number of free entries.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        toIEW->commitInfo[tid].usedROB = true;
        toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);
        toIEW->commitInfo[tid].emptyROB = true;
    }

    // Commit must broadcast the number of free entries it has at the
    // start of the simulation, so it starts as active.
    cpu->activateStage(O3CPU::CommitIdx);

    cpu->activityThisCycle();
}

template <class Impl>
void
DefaultCommit<Impl>::drain()
{
    drainPending = true;
}

template <class Impl>
void
DefaultCommit<Impl>::drainResume()
{
    drainPending = false;
    drainImminent = false;
}

template <class Impl>
void
DefaultCommit<Impl>::drainSanityCheck() const
{
    assert(isDrained());
    rob->drainSanityCheck();
}

template <class Impl>
bool
DefaultCommit<Impl>::isDrained() const
{
    /* Make sure no one is executing microcode. There are two reasons
     * for this:
     * - Hardware virtualized CPUs can't switch into the middle of a
     *   microcode sequence.
     * - The current fetch implementation will most likely get very
     *   confused if it tries to start fetching an instruction that
     *   is executing in the middle of a ucode sequence that changes
     *   address mappings. This can happen on for example x86.
     */
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (pc[tid].microPC() != 0)
            return false;
    }

    /* Make sure that all instructions have finished committing before
     * declaring the system as drained. We want the pipeline to be
     * completely empty when we declare the CPU to be drained. This
     * makes debugging easier since CPU handover and restoring from a
     * checkpoint with a different CPU should have the same timing.
     */
    return rob->isEmpty() &&
        interrupt == NoFault;
}

template <class Impl>
void
DefaultCommit<Impl>::takeOverFrom()
{
    _status = Active;
    _nextStatus = Inactive;
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        commitStatus[tid] = Idle;
        changedROBNumEntries[tid] = false;
        trapSquash[tid] = false;
        tcSquash[tid] = false;
        squashAfterInst[tid] = NULL;
    }
    rob->takeOverFrom();
}

template <class Impl>
void
DefaultCommit<Impl>::deactivateThread(ThreadID tid)
{
    list<ThreadID>::iterator thread_it = std::find(priority_list.begin(),
            priority_list.end(), tid);

    if (thread_it != priority_list.end()) {
        priority_list.erase(thread_it);
    }
}


template <class Impl>
void
DefaultCommit<Impl>::updateStatus()
{
    // reset ROB changed variable
    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        changedROBNumEntries[tid] = false;

        // Also check if any of the threads has a trap pending
        if (commitStatus[tid] == TrapPending ||
            commitStatus[tid] == FetchTrapPending) {
            _nextStatus = Active;
        }
    }

    if (_nextStatus == Inactive && _status == Active) {
        DPRINTF(Activity, "Deactivating stage.\n");
        cpu->deactivateStage(O3CPU::CommitIdx);
    } else if (_nextStatus == Active && _status == Inactive) {
        DPRINTF(Activity, "Activating stage.\n");
        cpu->activateStage(O3CPU::CommitIdx);
    }

    _status = _nextStatus;
}

template <class Impl>
bool
DefaultCommit<Impl>::changedROBEntries()
{
    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (changedROBNumEntries[tid]) {
            return true;
        }
    }

    return false;
}

template <class Impl>
size_t
DefaultCommit<Impl>::numROBFreeEntries(ThreadID tid)
{
    return rob->numFreeEntries(tid);
}

template <class Impl>
void
DefaultCommit<Impl>::generateTrapEvent(ThreadID tid, Fault inst_fault)
{
    DPRINTF(Commit, "Generating trap event for [tid:%i]\n", tid);

    EventFunctionWrapper *trap = new EventFunctionWrapper(
        [this, tid]{ processTrapEvent(tid); },
        "Trap", true, Event::CPU_Tick_Pri);

    Cycles latency = dynamic_pointer_cast<SyscallRetryFault>(inst_fault) ?
                     cpu->syscallRetryLatency : trapLatency;

    cpu->schedule(trap, cpu->clockEdge(latency));
    trapInFlight[tid] = true;
    thread[tid]->trapPending = true;
}

template <class Impl>
void
DefaultCommit<Impl>::generateTCEvent(ThreadID tid)
{
    assert(!trapInFlight[tid]);
    DPRINTF(Commit, "Generating TC squash event for [tid:%i]\n", tid);

    tcSquash[tid] = true;
}

template <class Impl>
void
DefaultCommit<Impl>::squashAll(ThreadID tid)
{
    // If we want to include the squashing instruction in the squash,
    // then use one older sequence number.
    // Hopefully this doesn't mess things up.  Basically I want to squash
    // all instructions of this thread.
    InstSeqNum squashed_inst = rob->isEmpty(tid) ?
        lastCommitedSeqNum[tid] : rob->readHeadInst(tid)->seqNum - 1;

    // All younger instructions will be squashed. Set the sequence
    // number as the youngest instruction in the ROB (0 in this case.
    // Hopefully nothing breaks.)
    youngestSeqNum[tid] = lastCommitedSeqNum[tid];

    rob->squash(MisspredictionType::NONE, squashed_inst, tid);
    changedROBNumEntries[tid] = true;

    // Send back the sequence number of the squashed instruction.
    toIEW->commitInfo[tid].doneSeqNum = squashed_inst;

    // Send back the squash signal to tell stages that they should
    // squash.
    toIEW->commitInfo[tid].squash = true;

    // Send back the rob squashing signal so other stages know that
    // the ROB is in the process of squashing.
    toIEW->commitInfo[tid].robSquashing = true;

    toIEW->commitInfo[tid].mispredictInst = NULL;
    toIEW->commitInfo[tid].squashInst = NULL;

    toIEW->commitInfo[tid].pc = pc[tid];
}

template <class Impl>
void
DefaultCommit<Impl>::squashFromTrap(ThreadID tid)
{
    squashAll(tid);

    DPRINTF(Commit, "Squashing from trap, restarting at PC %s\n", pc[tid]);

    thread[tid]->trapPending = false;
    thread[tid]->noSquashFromTC = false;
    trapInFlight[tid] = false;

    trapSquash[tid] = false;

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();
}

template <class Impl>
void
DefaultCommit<Impl>::squashFromTC(ThreadID tid)
{
    squashAll(tid);

    DPRINTF(Commit, "Squashing from TC, restarting at PC %s\n", pc[tid]);

    thread[tid]->noSquashFromTC = false;
    assert(!thread[tid]->trapPending);

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();

    tcSquash[tid] = false;
}

template <class Impl>
void
DefaultCommit<Impl>::squashFromSquashAfter(ThreadID tid)
{
    DPRINTF(Commit, "Squashing after squash after request, "
            "restarting at PC %s\n", pc[tid]);

    squashAll(tid);
    // Make sure to inform the fetch stage of which instruction caused
    // the squash. It'll try to re-fetch an instruction executing in
    // microcode unless this is set.
    toIEW->commitInfo[tid].squashInst = squashAfterInst[tid];
    squashAfterInst[tid] = NULL;

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();
}

template <class Impl>
void
DefaultCommit<Impl>::squashAfter(ThreadID tid, DynInstPtr &head_inst)
{
    DPRINTF(Commit, "Executing squash after for [tid:%i] inst [sn:%lli]\n",
            tid, head_inst->seqNum);

    assert(!squashAfterInst[tid] || squashAfterInst[tid] == head_inst);
    commitStatus[tid] = SquashAfterPending;
    squashAfterInst[tid] = head_inst;
}

template <class Impl>
void
DefaultCommit<Impl>::tick()
{
    wroteToTimeBuffer = false;
    _nextStatus = Inactive;

    if (activeThreads->empty())
        return;

    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    // Check if any of the threads are done squashing.  Change the
    // status if they are done.
    while (threads != end) {
        ThreadID tid = *threads++;

        // Clear the bit saying if the thread has committed stores
        // this cycle.
        committedStores[tid] = false;

        if (commitStatus[tid] == ROBSquashing) {

            if (rob->isDoneSquashing(tid)) {
                commitStatus[tid] = Running;
            } else {
                DPRINTF(Commit,"[tid:%u]: Still Squashing, cannot commit any"
                        " insts this cycle.\n", tid);
                rob->doSquash(tid);
                toIEW->commitInfo[tid].robSquashing = true;
                wroteToTimeBuffer = true;
            }
        }
    }

    commit();

    markCompletedInsts();

    threads = activeThreads->begin();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (!rob->isEmpty(tid) && rob->readHeadInst(tid)->readyToCommit()) {
            // The ROB has more instructions it can commit. Its next status
            // will be active.
            _nextStatus = Active;

            DynInstPtr inst = rob->readHeadInst(tid);

            DPRINTF(Commit,"[tid:%i]: Instruction [sn:%lli] PC %s is head of"
                    " ROB and ready to commit\n",
                    tid, inst->seqNum, inst->pcState());

        } else if (!rob->isEmpty(tid)) {
            DynInstPtr inst = rob->readHeadInst(tid);

            ppCommitStall->notify(inst);

            DPRINTF(Commit,"[tid:%i]: Can't commit, Instruction [sn:%lli] PC "
                    "%s is head of ROB and not ready\n",
                    tid, inst->seqNum, inst->pcState());
        }

        DPRINTF(Commit, "[tid:%i]: ROB has %d insts & %d free entries.\n",
                tid, rob->countInsts(tid), rob->numFreeEntries(tid));
    }


    if (wroteToTimeBuffer) {
        DPRINTF(Activity, "Activity This Cycle.\n");
        cpu->activityThisCycle();
    }

    updateStatus();
}

template <class Impl>
void
DefaultCommit<Impl>::handleInterrupt()
{
    // Verify that we still have an interrupt to handle
    if (!cpu->checkInterrupts(cpu->tcBase(0))) {
        DPRINTF(Commit, "Pending interrupt is cleared by master before "
                "it got handled. Restart fetching from the orig path.\n");
        toIEW->commitInfo[0].clearInterrupt = true;
        interrupt = NoFault;
        avoidQuiesceLiveLock = true;
        return;
    }

    // Wait until all in flight instructions are finished before enterring
    // the interrupt.
    if (canHandleInterrupts && cpu->instList.empty()) {
        // Squash or record that I need to squash this cycle if
        // an interrupt needed to be handled.
        DPRINTF(Commit, "Interrupt detected.\n");

        // Clear the interrupt now that it's going to be handled
        toIEW->commitInfo[0].clearInterrupt = true;

        assert(!thread[0]->noSquashFromTC);
        thread[0]->noSquashFromTC = true;

        if (cpu->checker) {
            cpu->checker->handlePendingInt();
        }

        // CPU will handle interrupt. Note that we ignore the local copy of
        // interrupt. This is because the local copy may no longer be the
        // interrupt that the interrupt controller thinks is being handled.
        cpu->processInterrupts(cpu->getInterrupts());

        thread[0]->noSquashFromTC = false;

        commitStatus[0] = TrapPending;

        interrupt = NoFault;

        // Generate trap squash event.
        generateTrapEvent(0, interrupt);

        avoidQuiesceLiveLock = false;
    } else {
        DPRINTF(Commit, "Interrupt pending: instruction is %sin "
                "flight, ROB is %sempty\n",
                canHandleInterrupts ? "not " : "",
                cpu->instList.empty() ? "" : "not " );
    }
}

template <class Impl>
void
DefaultCommit<Impl>::propagateInterrupt()
{
    // Don't propagate intterupts if we are currently handling a trap or
    // in draining and the last observable instruction has been committed.
    if (commitStatus[0] == TrapPending || interrupt || trapSquash[0] ||
            tcSquash[0] || drainImminent)
        return;

    // Process interrupts if interrupts are enabled, not in PAL
    // mode, and no other traps or external squashes are currently
    // pending.
    // @todo: Allow other threads to handle interrupts.

    // Get any interrupt that happened
    interrupt = cpu->getInterrupts();

    // Tell fetch that there is an interrupt pending.  This
    // will make fetch wait until it sees a non PAL-mode PC,
    // at which point it stops fetching instructions.
    if (interrupt != NoFault)
        toIEW->commitInfo[0].interruptPending = true;
}

template <class Impl>
void
DefaultCommit<Impl>::commit()
{
    if (FullSystem) {
        // Check if we have a interrupt and get read to handle it
        if (cpu->checkInterrupts(cpu->tcBase(0)))
            propagateInterrupt();
    }

    ////////////////////////////////////
    // Check for any possible squashes, handle them first
    ////////////////////////////////////
    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    int num_squashing_threads = 0;

    while (threads != end) {
        ThreadID tid = *threads++;

        // Not sure which one takes priority.  I think if we have
        // both, that's a bad sign.
        if (trapSquash[tid]) {
            assert(!tcSquash[tid]);
            squashFromTrap(tid);
        } else if (tcSquash[tid]) {
            assert(commitStatus[tid] != TrapPending);
            squashFromTC(tid);
        } else if (commitStatus[tid] == SquashAfterPending) {
            // A squash from the previous cycle of the commit stage (i.e.,
            // commitInsts() called squashAfter) is pending. Squash the
            // thread now.
            squashFromSquashAfter(tid);
        }

        // Squashed sequence number must be older than youngest valid
        // instruction in the ROB. This prevents squashes from younger
        // instructions overriding squashes from older instructions.
        if (fromIEW->squash[tid] &&
            commitStatus[tid] != TrapPending &&
            fromIEW->squashedSeqNum[tid] <= youngestSeqNum[tid]) {

            if (fromIEW->mispredictInst[tid]) {
                DPRINTF(Commit,
                  "[tid:%i]: Squashing due to branch mispred PC:%#x [sn:%i]\n",
                    tid,
                    fromIEW->mispredictInst[tid]->instAddr(),
                    fromIEW->squashedSeqNum[tid]);
            } else {
                DPRINTF(Commit,
                    "[tid:%i]: Squashing due to order violation [sn:%i]\n",
                    tid, fromIEW->squashedSeqNum[tid]);
            }

            DPRINTF(Commit, "[tid:%i]: Redirecting to PC %#x\n",
                    tid,
                    fromIEW->pc[tid].nextInstAddr());

            commitStatus[tid] = ROBSquashing;

            // If we want to include the squashing instruction in the squash,
            // then use one older sequence number.
            InstSeqNum squashed_inst = fromIEW->squashedSeqNum[tid];

            if (fromIEW->includeSquashInst[tid]) {
                squashed_inst--;
            }

            // All younger instructions will be squashed. Set the sequence
            // number as the youngest instruction in the ROB.
            youngestSeqNum[tid] = squashed_inst;

            rob->squash(
                  fromIEW->squashMisspredictionType[tid], squashed_inst, tid);
            changedROBNumEntries[tid] = true;

            toIEW->commitInfo[tid].doneSeqNum = squashed_inst;

            toIEW->commitInfo[tid].squash = true;
            toIEW->commitInfo[tid].squashedPID = fromIEW->squashedPID[tid];
            toIEW->commitInfo[tid].squashDueToMispredictedPID =
                                    fromIEW->squashDueToMispredictedPID[tid];
            toIEW->commitInfo[tid].squashMisspredictionType =
                                    fromIEW->squashMisspredictionType[tid];
            // Send back the rob squashing signal so other stages know that
            // the ROB is in the process of squashing.
            toIEW->commitInfo[tid].robSquashing = true;

            toIEW->commitInfo[tid].mispredictInst =
                fromIEW->mispredictInst[tid];
            toIEW->commitInfo[tid].branchTaken =
                fromIEW->branchTaken[tid];
            toIEW->commitInfo[tid].squashInst =
                                    rob->findInst(tid, squashed_inst);
            if (toIEW->commitInfo[tid].mispredictInst) {
                if (toIEW->commitInfo[tid].mispredictInst->isUncondCtrl()) {
                     toIEW->commitInfo[tid].branchTaken = true;
                }
                ++branchMispredicts;
            }

            toIEW->commitInfo[tid].pc = fromIEW->pc[tid];
        }

        if (commitStatus[tid] == ROBSquashing) {
            num_squashing_threads++;
        }
    }

    // If commit is currently squashing, then it will have activity for the
    // next cycle. Set its next status as active.
    if (num_squashing_threads) {
        _nextStatus = Active;
    }

    if (num_squashing_threads != numThreads) {
        // If we're not currently squashing, then get instructions.
        getInsts();

        // Try to commit any instructions.
        commitInsts();
    }

    //Check for any activity
    threads = activeThreads->begin();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (changedROBNumEntries[tid]) {
            toIEW->commitInfo[tid].usedROB = true;
            toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);

            wroteToTimeBuffer = true;
            changedROBNumEntries[tid] = false;
            if (rob->isEmpty(tid))
                checkEmptyROB[tid] = true;
        }

        // ROB is only considered "empty" for previous stages if: a)
        // ROB is empty, b) there are no outstanding stores, c) IEW
        // stage has received any information regarding stores that
        // committed.
        // c) is checked by making sure to not consider the ROB empty
        // on the same cycle as when stores have been committed.
        // @todo: Make this handle multi-cycle communication between
        // commit and IEW.
        if (checkEmptyROB[tid] && rob->isEmpty(tid) &&
            !iewStage->hasStoresToWB(tid) && !committedStores[tid]) {
            checkEmptyROB[tid] = false;
            toIEW->commitInfo[tid].usedROB = true;
            toIEW->commitInfo[tid].emptyROB = true;
            toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);
            wroteToTimeBuffer = true;
        }

    }
}

template <class Impl>
void
DefaultCommit<Impl>::commitInsts()
{
    ////////////////////////////////////
    // Handle commit
    // Note that commit will be handled prior to putting new
    // instructions in the ROB so that the ROB only tries to commit
    // instructions it has in this current cycle, and not instructions
    // it is writing in during this cycle.  Can't commit and squash
    // things at the same time...
    ////////////////////////////////////

    DPRINTF(Commit, "Trying to commit instructions in the ROB.\n");

    unsigned num_committed = 0;

    DynInstPtr head_inst;

    // Commit as many instructions as possible until the commit bandwidth
    // limit is reached, or it becomes impossible to commit any more.
    while (num_committed < commitWidth) {
        // Check for any interrupt that we've already squashed for
        // and start processing it.
        if (interrupt != NoFault)
            handleInterrupt();

        ThreadID commit_thread = getCommittingThread();

        if (commit_thread == -1 || !rob->isHeadReady(commit_thread))
            break;

        head_inst = rob->readHeadInst(commit_thread);

        ThreadID tid = head_inst->threadNumber;

        assert(tid == commit_thread);

        DPRINTF(Commit, "Trying to commit head instruction, [sn:%i] [tid:%i]\n",
                head_inst->seqNum, tid);

        // If the head instruction is squashed, it is ready to retire
        // (be removed from the ROB) at any time.
        if (head_inst->isSquashed()) {

            squashExecuteAliasTable(head_inst);

            DPRINTF(Commit, "Retiring squashed instruction from "
                    "ROB.\n");

            rob->retireHead(commit_thread);

            ++commitSquashedInsts;
            if (head_inst->MissPIDSquashType != MisspredictionType::NONE){
                ++commitSquashedInstsDueToMissPID;
            }
            // Notify potential listeners that this instruction is squashed
            ppSquash->notify(head_inst);

            // Record that the number of ROB entries has changed.
            changedROBNumEntries[tid] = true;
        } else {
            pc[tid] = head_inst->pcState();

            // Increment the total number of non-speculative instructions
            // executed.
            // Hack for now: it really shouldn't happen until after the
            // commit is deemed to be successful, but this count is needed
            // for syscalls.
            thread[tid]->funcExeInst++;

            // Try to commit the head instruction.
            bool commit_success = commitHead(head_inst, num_committed);



            if (commit_success) {
                ++num_committed;
                statCommittedInstType[tid][head_inst->opClass()]++;
                ppCommit->notify(head_inst);

                changedROBNumEntries[tid] = true;

                // Set the doneSeqNum to the youngest committed instruction.
                toIEW->commitInfo[tid].doneSeqNum = head_inst->seqNum;

                if (tid == 0) {
                    canHandleInterrupts =  (!head_inst->isDelayedCommit()) &&
                                           ((THE_ISA != ALPHA_ISA) ||
                                             (!(pc[0].instAddr() & 0x3)));
                }

                // at this point store conditionals should either have
                // been completed or predicated false
                assert(!head_inst->isStoreConditional() ||
                       head_inst->isCompleted() ||
                       !head_inst->readPredicate());

                // Updates misc. registers.
                head_inst->updateMiscRegs();

                // Check instruction execution if it successfully commits and
                // is not carrying a fault.
                if (cpu->checker) {
                    cpu->checker->verify(head_inst);
                }

                cpu->traceFunctions(pc[tid].instAddr());

                TheISA::advancePC(pc[tid], head_inst->staticInst);

                // Keep track of the last sequence number commited
                lastCommitedSeqNum[tid] = head_inst->seqNum;

                // If this is an instruction that doesn't play nicely with
                // others squash everything and restart fetch
                if (head_inst->isSquashAfter())
                    squashAfter(tid, head_inst);

                if (drainPending) {
                    if (pc[tid].microPC() == 0 && interrupt == NoFault &&
                        !thread[tid]->trapPending) {
                        // Last architectually committed instruction.
                        // Squash the pipeline, stall fetch, and use
                        // drainImminent to disable interrupts
                        DPRINTF(Drain, "Draining: %i:%s\n", tid, pc[tid]);
                        squashAfter(tid, head_inst);
                        cpu->commitDrained(tid);
                        drainImminent = true;
                    }
                }

                bool onInstBoundary = !head_inst->isMicroop() ||
                                      head_inst->isLastMicroop() ||
                                      !head_inst->isDelayedCommit();

                if (onInstBoundary) {
                    int count = 0;
                    Addr oldpc;
                    // Make sure we're not currently updating state while
                    // handling PC events.
                    assert(!thread[tid]->noSquashFromTC &&
                           !thread[tid]->trapPending);
                    do {
                        oldpc = pc[tid].instAddr();
                        cpu->system->pcEventQueue.service(thread[tid]->getTC());
                        count++;
                    } while (oldpc != pc[tid].instAddr());
                    if (count > 1) {
                        DPRINTF(Commit,
                                "PC skip function event, stopping commit\n");
                        break;
                    }
                }

                // Check if an instruction just enabled interrupts and we've
                // previously had an interrupt pending that was not handled
                // because interrupts were subsequently disabled before the
                // pipeline reached a place to handle the interrupt. In that
                // case squash now to make sure the interrupt is handled.
                //
              // If we don't do this, we might end up in a live lock situation
                if (!interrupt && avoidQuiesceLiveLock &&
                    onInstBoundary && cpu->checkInterrupts(cpu->tcBase(0)))
                    squashAfter(tid, head_inst);
            } else {
                DPRINTF(Commit, "Unable to commit head instruction PC:%s "
                        "[tid:%i] [sn:%i].\n",
                        head_inst->pcState(), tid ,head_inst->seqNum);
                break;
            }
        }
    }

    DPRINTF(CommitRate, "%i\n", num_committed);
    numCommittedDist.sample(num_committed);

    if (num_committed == commitWidth) {
        commitEligibleSamples++;
    }
}

template <class Impl>
bool
DefaultCommit<Impl>::commitHead(DynInstPtr &head_inst, unsigned inst_num)
{
    assert(head_inst);

    ThreadID tid = head_inst->threadNumber;

    // If the instruction is not executed yet, then it will need extra
    // handling.  Signal backwards that it should be executed.
    ThreadContext * tc = cpu->tcBase(tid);


    if (!head_inst->isExecuted()) {
        // Keep this number correct.  We have not yet actually executed
        // and committed this instruction.
        thread[tid]->funcExeInst--;

        // Make sure we are only trying to commit un-executed instructions we
        // think are possible.
        assert(head_inst->isNonSpeculative() || head_inst->isStoreConditional()
               || head_inst->isMemBarrier() || head_inst->isWriteBarrier() ||
               (head_inst->isLoad() && head_inst->strictlyOrdered()));

        DPRINTF(Commit, "Encountered a barrier or non-speculative "
                "instruction [sn:%lli] at the head of the ROB, PC %s.\n",
                head_inst->seqNum, head_inst->pcState());

        if (inst_num > 0 || iewStage->hasStoresToWB(tid)) {
            DPRINTF(Commit, "Waiting for all stores to writeback.\n");
            return false;
        }

        toIEW->commitInfo[tid].nonSpecSeqNum = head_inst->seqNum;

        // Change the instruction so it won't try to commit again until
        // it is executed.
        head_inst->clearCanCommit();

        if (head_inst->isLoad() && head_inst->strictlyOrdered()) {
            DPRINTF(Commit, "[sn:%lli]: Strictly ordered load, PC %s.\n",
                    head_inst->seqNum, head_inst->pcState());
            toIEW->commitInfo[tid].strictlyOrdered = true;
            toIEW->commitInfo[tid].strictlyOrderedLoad = head_inst;
        } else {
            ++commitNonSpecStalls;
        }

        return false;
    }

    if (head_inst->isThreadSync()) {
        // Not handled for now.
        panic("Thread sync instructions are not handled yet.\n");
    }

    // Check if the instruction caused a fault.  If so, trap.
    Fault inst_fault = head_inst->getFault();

    // Stores mark themselves as completed.
    if (!head_inst->isStore() && inst_fault == NoFault) {
        head_inst->setCompleted();
    }

    if (inst_fault != NoFault) {
        DPRINTF(Commit, "Inst [sn:%lli] PC %s has a fault\n",
                head_inst->seqNum, head_inst->pcState());

        if (iewStage->hasStoresToWB(tid) || inst_num > 0) {
            DPRINTF(Commit, "Stores outstanding, fault must wait.\n");
            return false;
        }

        head_inst->setCompleted();

        // If instruction has faulted, let the checker execute it and
        // check if it sees the same fault and control flow.
        if (cpu->checker) {
            // Need to check the instruction before its fault is processed
            cpu->checker->verify(head_inst);
        }

        assert(!thread[tid]->noSquashFromTC);

        // Mark that we're in state update mode so that the trap's
        // execution doesn't generate extra squashes.
        thread[tid]->noSquashFromTC = true;

        // Execute the trap.  Although it's slightly unrealistic in
        // terms of timing (as it doesn't wait for the full timing of
        // the trap event to complete before updating state), it's
        // needed to update the state as soon as possible.  This
        // prevents external agents from changing any specific state
        // that the trap need.
        cpu->trap(inst_fault, tid,
                  head_inst->notAnInst() ?
                      StaticInst::nullStaticInstPtr :
                      head_inst->staticInst);

        // Exit state update mode to avoid accidental updating.
        thread[tid]->noSquashFromTC = false;

        commitStatus[tid] = TrapPending;

        DPRINTF(Commit, "Committing instruction with fault [sn:%lli]\n",
            head_inst->seqNum);
        if (head_inst->traceData) {
            if (DTRACE(ExecFaulting)) {
                head_inst->traceData->setFetchSeq(head_inst->seqNum);
                head_inst->traceData->setCPSeq(thread[tid]->numOp);
                head_inst->traceData->dump();
            }
            delete head_inst->traceData;
            head_inst->traceData = NULL;
        }

        // Generate trap squash event.
        generateTrapEvent(tid, inst_fault);
        return false;
    }

    updateComInstStats(head_inst);

    if (FullSystem) {
        if (thread[tid]->profile) {
            thread[tid]->profilePC = head_inst->instAddr();
            ProfileNode *node = thread[tid]->profile->consume(
                    thread[tid]->getTC(), head_inst->staticInst);

            if (node)
                thread[tid]->profileNode = node;
        }
        if (CPA::available()) {
            if (head_inst->isControl()) {
                ThreadContext *tc = thread[tid]->getTC();
                CPA::cpa()->swAutoBegin(tc, head_inst->nextInstAddr());
            }
        }
    }


    const StaticInstPtr si = head_inst->staticInst;
    if (head_inst->isMicroopInjected()){
       collector(tid, head_inst);
    }

    DPRINTF(Commit, "Committing instruction with [sn:%lli] PC %s %s\n",
                    head_inst->seqNum, head_inst->pcState(),
                    si->disassemble(head_inst->pcState().pc())
                  );

    DPRINTF(AliasCache, "State of Aliases before Removel of Stack Aliases:\n");
    cpu->ExeAliasCache->DumpAliasTableBuffer();
    cpu->ExeAliasCache->DumpAliasCache();
    cpu->ExeAliasCache->DumpShadowMemory(tc);

    if (tc->enableCapability){
      cpu->updatePIDHistory(head_inst);
    }

        
    if (tc->enableCapability){

        if (head_inst->isStore())
        {
            CommitUpdateAliasTableInCommit(tid, head_inst);
        }
        else
        {
            cpu->PointerDepGraph.updatePIDWithTypeTracker(head_inst, tc);
        }
        
        DPRINTF(AliasCache, "Stack Top: Inst: %x CPU: %x\n", 
                findCurrentStackTop(head_inst), cpu->readArchIntReg(X86ISA::INTREG_RSP, tid));

        Addr currStackTop = findCurrentStackTop(head_inst);
        if (currStackTop != 0)
            cpu->ExeAliasCache->RemoveStackAliases(currStackTop, tc);

    }

    DPRINTF(AliasCache, "State of Aliases after Removel of Stack Aliases:\n");
    cpu->ExeAliasCache->DumpAliasTableBuffer();
    cpu->ExeAliasCache->DumpAliasCache();
    cpu->ExeAliasCache->DumpShadowMemory(tc);

    if (tc->enableCapability &&
        cpu->fetch.TrackAlias(tc, head_inst->pcState().pc()))
    {
        cpu->PointerDepGraph.doCommit(head_inst);
    }

    // perform this after RAX is updated by doCommit
    if (tc->enableCapability &&
        (head_inst->isReallocBaseCollectorMicroop() ||
        head_inst->isMallocBaseCollectorMicroop() ||
        head_inst->isCallocBaseCollectorMicroop()))
    {
        cpu->PointerDepGraph.updatePointerTrackerForAPBaseCollectorMicroops(head_inst, tc);
    }


    if (head_inst->traceData) {
        head_inst->traceData->setFetchSeq(head_inst->seqNum);
        head_inst->traceData->setCPSeq(thread[tid]->numOp);
        head_inst->traceData->dump();

        if (tc->enableCapability &&
            (head_inst->isLoad() || head_inst->isStore()) &&
            (!head_inst->isMicroopInjected()) && 
            cpu->fetch.TrackAlias(tc, head_inst->pcState().pc())
            )
        {
            cpu->PointerDepGraph.checkTyCHESanity(head_inst, tc);
        }
        
        delete head_inst->traceData;
        head_inst->traceData = NULL;
    }




    if (head_inst->isStore())
    {
        std::ofstream tyCHEExecutionSanityCheck;
        if (tc->enableCapability)
            tyCHEExecutionSanityCheck.open("./m5out/ExecSanityTyche.tyche", std::ios_base::app);
        else 
            tyCHEExecutionSanityCheck.open("./m5out/ExecSanityRaw.tyche", std::ios_base::app);

        tyCHEExecutionSanityCheck << std::hex << head_inst->instAddr() << " " << 
                                     std::dec << (uint64_t)cpu->committedOps[tid].value() <<
                                     std::endl;
        tyCHEExecutionSanityCheck.close();
    }

    if (head_inst->isReturn()) {
        DPRINTF(Commit,"Return Instruction Committed [sn:%lli] PC %s  \
                        --- NextPC: %#lx RAX: %#lx \n",
                        head_inst->seqNum, head_inst->pcState(),
                        head_inst->pcState().npc() ,
                        cpu->readArchIntReg(X86ISA::INTREG_RAX, tid));
    }

    // logic for updating activation records
    if (tc->enableCapability && false)
    {
        if (head_inst->isCall())
        {
            if(tc->FunctionObjectsBuffer.find(head_inst->pcState().npc()) != tc->FunctionObjectsBuffer.end())
            {
                DPRINTF(TrackFunctionObject,"Call Instruction Committed [sn:%lli] PC= %s "
                                " NextPC: %#lx\n",
                                head_inst->seqNum, head_inst->pcState(),
                                head_inst->pcState().npc());
                FunctionProfile* fp = new FunctionProfile(cpu->params()->system->kernelSymtab, 
                                                        tc->FunctionObjectsBuffer[head_inst->pcState().npc()]);
                cpu->thread[0]->activationRecords.push(fp);
            }
        }
        else if (!cpu->thread[0]->activationRecords.empty() && 
                 head_inst->isReturn())
        {
            FunctionProfile* fp =  cpu->thread[0]->activationRecords.top();
            cpu->thread[0]->activationRecords.pop();
            delete fp;
        }
        else if (!cpu->thread[0]->activationRecords.empty() &&
                (head_inst->isLoad() || head_inst->isStore()))
        {
            updateFunctionActivationRecord(head_inst);
        }

    }

    if (tc->enableCapability){


        if ((uint64_t)cpu->thread[tid]->numInsts.value() % 100000000 == 0 &&
            !head_inst->isNop() &&
            !head_inst->isInstPrefetch() &&
            head_inst->isLastMicroop()
           )
        {

            std::cout <<
            "--------------------START OF EPOCH----------------------------" <<
            std::endl << std::dec << cpu->thread[tid]->numInsts.value() <<
            std::endl <<
            " NumOfAllocations: " <<
            tc->num_of_allocations << std::endl <<
            " ShadowMemory Size: " <<
            tc->ShadowMemory.size() <<  std::endl <<
            " Alias Cache SQ Size: " <<
            cpu->ExeAliasCache->GetSize() << std::endl <<
            std::endl;

            double accuracy =
            (double)(cpu->NumOfAliasTableAccess - cpu->FalsePredict) /
            cpu->NumOfAliasTableAccess;
            //
             std::cout << std::dec << //cpu->thread[tid]->numInsts.value() <<
            " Number of Execute Alias Table accesses: " <<
            cpu->NumOfAliasTableAccess << std::endl <<
            " Prediction Accuracy(1e6 Instr.): " << accuracy << std::endl <<
            " Predictor Average Confidence Level(1e6 Instr.): " <<
            cpu->getLVPTAveConfidenceLevel() << std::endl <<
            " NumOfMissPredictions: " << cpu->FalsePredict <<std::endl <<
            " P0An: " << cpu->P0An <<
            " PnA0: " << cpu->PnA0 <<
            " PmAn: " << cpu->PmAn <<std::endl <<
            " Number Of Mem Refs: " << cpu->numOfMemRefs <<std::endl <<
          //  " Heap Access: " << cpu->heapAccesses <<
            //" True Predections: " << cpu->truePredection <<
            //" PnA0: " << cpu->HeapPnA0 <<
            //" PnAm: " << cpu->HeapPnAm <<std::endl <<
            //" Pointer Tracker Prediction Accuracy: " <<
            //  (double)cpu->truePredection/cpu->numOfMemRefs <<std::endl <<
            " NumOfInjectedBoundsCheck: " <<
            cpu->NumOfInjectedBoundsCheck <<std::endl <<
            " NumOfExecutedBoundsCheck: " <<
            cpu->NumOfExecutedBoundsCheck <<std::endl <<
            " NumOfCommitedBoundsCheck: " <<
            cpu->NumOfCommitedBoundsCheck <<std::endl <<
            " numOfCommitedMemRefs: " <<
            cpu->numOfCommitedMemRefs <<
            std::endl;
            tc->LRUPidCache.LRUPIDCachePrintStats();
            cpu->ExeAliasCache->print_stats();

            // final stats
            cpu->numOfCapabilityCheckMicroops += cpu->NumOfCommitedBoundsCheck;

            cpu->numOutStandingCapabilityCacheAccesses =
                                      tc->LRUPidCache.total_misses;
            cpu->numOutStandingReadAliasCacheAccesses =
                                      cpu->ExeAliasCache->outstandingRead;
            cpu->numOutStandingWriteAliasCacheAccesses =
                                      cpu->ExeAliasCache->outstandingWrite;

            cpu->numCapabilityCacheMisses = tc->LRUPidCache.total_misses;
            cpu->numCapabilityCacheAccesses = tc->LRUPidCache.total_accesses;

            cpu->numAliasCacheMisses = cpu->ExeAliasCache->total_misses;
            cpu->numAliasCacheAccesses = cpu->ExeAliasCache->total_accesses;

            //transient stats
            cpu->NumOfAliasTableAccess=0; cpu->FalsePredict=0;
            cpu->PnA0 = 0; cpu->P0An=0; cpu->PmAn = 0;
            cpu->heapAccesses = 0; cpu->truePredection = 0;
            cpu->numOfMemRefs = 0; cpu->HeapPnAm = 0; cpu->HeapPnA0 = 0;
            cpu->NumOfCommitedBoundsCheck = 0;
            cpu->NumOfInjectedBoundsCheck = 0;
            cpu->NumOfExecutedBoundsCheck = 0;
            cpu->numOfCommitedMemRefs = 0;

            //cpu->dumpCapabilityStats();

        }
    }

    if (tc->enableCapability && head_inst->isBoundsCheckMicroop()){
        cpu->NumOfCommitedBoundsCheck++;
    }

    if ((tc->enableCapability) &&
        (head_inst->isMallocBaseCollectorMicroop() ||
        head_inst->isMallocSizeCollectorMicroop() ||
        head_inst->isCallocSizeCollectorMicroop() ||
        head_inst->isCallocBaseCollectorMicroop() ||
        head_inst->isReallocSizeCollectorMicroop() ||
        head_inst->isReallocBaseCollectorMicroop()))
    {
          cpu->numOfCapabilityGenMicroops++;
    }

    if ((tc->enableCapability) &&
        (head_inst->isFreeCallMicroop() ||
        head_inst->isFreeRetMicroop())
        ){
          cpu->numOfCapabilityFreeMicroops++;
    }

    if ((head_inst->isLoad() || head_inst->isStore())  &&
        !head_inst->isBoundsCheckMicroop())
        cpu->numOfCommitedMemRefs++;

    // Update the commit rename map
    for (int i = 0; i < head_inst->numDestRegs(); i++) {
        renameMap[tid]->setEntry(head_inst->flattenedDestRegIdx(i),
                                 head_inst->renamedDestRegIdx(i));
    }




    // Finally clear the head ROB entry.
    rob->retireHead(tid);



#if TRACING_ON
    if (DTRACE(O3PipeView)) {
        head_inst->commitTick = curTick() - head_inst->fetchTick;
    }
#endif
    head_inst->commitTick = curTick() - head_inst->fetchTick;
    // If this was a store, record it for this cycle.
    if (head_inst->isStore())
        committedStores[tid] = true;

    // Return true to indicate that we have committed an instruction.
    return true;
}

template <class Impl>
void
DefaultCommit<Impl>::collector(ThreadID tid, DynInstPtr &inst)
{

  ThreadContext * tc = cpu->tcBase(tid);

  if (tc->enableCapability){

    if (inst->isMallocSizeCollectorMicroop()){

        if (tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::NONE)
            panic("AP_MALLOC_SIZE_COLLECT: Invalid Status!");


        uint64_t _pid_num=cpu->readArchIntReg(X86ISA::INTREG_R16, tid) + 1;
        uint64_t _pid_size=inst->readDestReg(inst->staticInst.get(),0);
        tc->ap_size  = _pid_size;
        tc->ap_pid   = _pid_num;

        tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::MALLOC_SIZE;

        DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                "MALLOC SIZE=%d PID=%d SEQNUM=%d PCADDR=0x%x\n",
                _pid_size, _pid_num, inst->seqNum, inst->pcState().instAddr());


    }
    else if (inst->isMallocBaseCollectorMicroop()){

      if (tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::MALLOC_SIZE)
            panic("AP_MALLOC_BASE_COLLECT: Invalid Status!");

        uint64_t _pid_num  = cpu->readArchIntReg(X86ISA::INTREG_R16, tid);
        uint64_t _pid_base = inst->readDestReg(inst->staticInst.get(),0);

        DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                "MALLOC BASE=0x%x PID=%d SEQNUM=%d PCADDR=0x%x\n",
                _pid_base, _pid_num, inst->seqNum, inst->pcState().instAddr());

        assert(_pid_num == tc->ap_pid);

        Block* bk = new Block();
        bk->payload   = (Addr)_pid_base;
        bk->req_szB   = (SizeT)tc->ap_size;
        bk->pid       = (Addr)_pid_num;
        bk->tid       = (Addr)inst->pcState().instAddr();
        bk->seqNum    = inst->seqNum;
        unsigned char present =
                      VG_addToFM(tc->interval_tree, (UWord)bk, (UWord)0);
        assert(!present);

        tc->num_of_allocations++;
        tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::NONE;


    }
    else if (inst->isFreeCallMicroop()){

        tc->free_base   = inst->readDestReg(inst->staticInst.get(),0);
        tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::FREE_CALL;

        DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                    "FREE CALL=0x%x SEQNUM=%d PCADDR=0x%x\n",
                    tc->free_base, inst->seqNum, inst->pcState().instAddr());
      

    }
    else if (inst->isFreeRetMicroop())
    {

        panic_if(tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::FREE_CALL, 
                "AP_FREE_RET: Invalid Status! Prev Status: %s\n", tc->Collector_Status);

        //check whether we have the cap for this AP or not
        TheISA::PointerID _pid = TheISA::PointerID(0);
        Block* bk = NULL;
        Block fake;
        fake.payload = tc->free_base;
        fake.req_szB = 1;
        UWord oldKeyW;
        unsigned char found = VG_delFromFM( tc->interval_tree,
                                     &oldKeyW, NULL, (Addr)&fake );
        if (found){
            bk = (Block*)oldKeyW;
            assert(bk);
            assert(bk->pid != 0);
            _pid = TheISA::PointerID(bk->pid);
            free(bk);
            assert(tc->num_of_allocations >= 1 && "tc->num_of_allocations < 1");
            tc->num_of_allocations--;
            
        }

        if (_pid != TheISA::PointerID(0))
        {
            // cpu->ExeAliasCache->Invalidate(tc, _pid);

            DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                    "FREE RET=0x%x PID=%d SEQNUM=%d PCADDR=0x%x\n",
                    tc->free_base, _pid, inst->seqNum, inst->pcState().instAddr());
        }

        DPRINTF(Allocator, "DefaultCommit<Impl>::collector::FREE RET\n");
        tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::NONE;

    }
    else if (inst->isCallocSizeCollectorMicroop()){

      if (tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::NONE)
         panic("AP_CALLOC_SIZE_COLLECT: Invalid Status!");

      uint64_t _pid_num = cpu->readArchIntReg(X86ISA::INTREG_R16, tid) + 1;
      uint64_t _pid_size_arg1 = inst->readDestReg(inst->staticInst.get(),0);
      uint64_t _pid_size_arg2 = inst->readIntRegOperand(inst->staticInst.get(),0);
      tc->ap_size = _pid_size_arg1 * _pid_size_arg2;
      tc->ap_pid = _pid_num;
      tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE;

      DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                "CALLOC SIZE=%d PID=%d SEQNUM=%d PCADDR=0x%x\n",
                tc->ap_size, _pid_num, inst->seqNum, inst->pcState().instAddr());

    }
    else if (inst->isCallocBaseCollectorMicroop()){

       if (tc->Collector_Status !=
                        ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE)
          panic("AP_CALLOC_BASE_COLLECT: Invalid Status!");

         uint64_t _pid_num  = cpu->readArchIntReg(X86ISA::INTREG_R16, tid);
         uint64_t _pid_base = inst->readDestReg(inst->staticInst.get(),0);

         DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                "CALLOC BASE=0x%x PID=%d SEQNUM=%d PCADDR=0x%x\n",
                _pid_base, _pid_num, inst->seqNum, inst->pcState().instAddr());

         assert(_pid_num == tc->ap_pid);


         Block* bk = new Block();
         bk->payload   = (Addr)_pid_base;
         bk->req_szB   = (SizeT)tc->ap_size;
         bk->pid       = (Addr)_pid_num;
         bk->tid       = (Addr)inst->pcState().instAddr();
         bk->seqNum    = inst->seqNum;
         unsigned char present =
                      VG_addToFM(tc->interval_tree, (UWord)bk, (UWord)0);
         assert(!present);

         tc->num_of_allocations++;
         tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::NONE;
         // rax now is a pointer
         TheISA::PointerID _pid = TheISA::PointerID(_pid_num);
         //tc->PointerTrackerTable[X86ISA::INTREG_RAX] = _pid;

    }
    else if (inst->isReallocSizeCollectorMicroop()){

        if (tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::NONE)
            panic("AP_REALLOC_SIZE_COLLECT: Invalid Status!");

        uint64_t _pid_num = cpu->readArchIntReg(X86ISA::INTREG_R16, tid) + 1;
        uint64_t _pid_base_arg1 = inst->readDestReg(inst->staticInst.get(),0); //RDI
        uint64_t _pid_size_arg2 = inst->readIntRegOperand(inst->staticInst.get(),0); //RSI

        uint64_t old_base_addr = _pid_base_arg1;
        tc->ap_size = _pid_size_arg2;
        tc->ap_pid = _pid_num;
        tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::REALLOC_SIZE;

        DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                    "REALLOC SIZE=%d PID=%d SEQNUM=%d PCADDR=0x%x\n",
                    tc->ap_size, _pid_num, inst->seqNum, inst->pcState().instAddr());

        TheISA::PointerID _pid = TheISA::PointerID(0);
        Block fake;
        fake.payload = old_base_addr;
        fake.req_szB = 1;
        UWord oldKeyW;
        unsigned char found = VG_delFromFM(tc->interval_tree, &oldKeyW, NULL, (Addr)&fake );

        if (found){
            Block* bk = (Block*)oldKeyW;
            assert(bk);
            assert(bk->pid != 0);
            _pid = TheISA::PointerID(bk->pid);
            free(bk);
            tc->num_of_allocations--;
        }

        if (_pid != TheISA::PointerID(0))
        {
            // cpu->ExeAliasCache->Invalidate(tc, _pid);

            DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                    "REALLOC CALL=0x%x PID=%d SEQNUM=%d PCADDR=0x%x\n",
                    old_base_addr, _pid, inst->seqNum, inst->pcState().instAddr());
        }


    }
    else if (inst->isReallocBaseCollectorMicroop()){

        if (tc->Collector_Status != ThreadContext::COLLECTOR_STATUS::REALLOC_SIZE)
            panic("AP_REALLOC_BASE_COLLECT: Invalid Status!");

            uint64_t _pid_num  = cpu->readArchIntReg(X86ISA::INTREG_R16, tid);
            uint64_t _pid_base = inst->readDestReg(inst->staticInst.get(),0);

            DPRINTF(Allocator, "DefaultCommit<Impl>::collector::"
                "REALLOC BASE=0x%x PID=%d SEQNUM=%d PCADDR=0x%x\n",
                _pid_base, _pid_num, inst->seqNum, inst->pcState().instAddr());

            assert(_pid_num == tc->ap_pid);

            Block* bk = new Block();
            bk->payload   = (Addr)_pid_base;
            bk->req_szB   = (SizeT)tc->ap_size;
            bk->pid       = (Addr)_pid_num;
            bk->tid       = (Addr)inst->pcState().instAddr();
            bk->seqNum    = inst->seqNum;
            unsigned char present =
                      VG_addToFM(tc->interval_tree, (UWord)bk, (UWord)0);
            assert(!present);

            tc->num_of_allocations++;
            tc->Collector_Status = ThreadContext::COLLECTOR_STATUS::NONE;

            assert(0);
    }
  }

}

template <class Impl>
void
DefaultCommit<Impl>::CommitUpdateAliasTableInCommit(ThreadID tid, DynInstPtr &head_inst)
{

  ThreadContext * tc = cpu->tcBase(tid); 
  assert(head_inst->isStore());
  assert(tc->enableCapability);
  assert (!head_inst->isMicroopInjected()) ;
  assert (!head_inst->isBoundsCheckMicroop()) ;


  const StaticInstPtr si = head_inst->staticInst;
  
  if (si->getName() != "st" && si->getName() != "stis") return;
  // datasize should be 8 bytes othersiwe it's not a base address
  if (si->getDataSize() != 8) return; // only for 64 bits system
       // return if store is not pointed to the DS or SS section
  if (!( si->getSegment() == TheISA::SEGMENT_REG_DS ||
            si->getSegment() == TheISA::SEGMENT_REG_SS)) return;

  // srcReg[2] in store microops is the register that
  //we want to write its value to mem
  uint64_t  dataRegContent = 
            head_inst->readIntRegOperand(head_inst->staticInst.get(),2); // src(2) is the data register

  DPRINTF(TypeTracker, "CommitUpdateAliasTableUsingPointerTracker: Inst[%lli]: Updating Alias[%x] = %d (spilled ptr=%x)\n", head_inst->seqNum, head_inst->effAddr, head_inst->dyn_pid, dataRegContent);
  // first call updatePIDWithTypeTracker to make sure we have the latest PID from type tracker
  cpu->PointerDepGraph.updatePIDWithTypeTracker(head_inst, tc);
  // update all the entrys in the AliasCache to make sure we are in sync
  //cpu->ExeAliasCache->UpdateEntry(head_inst, tc);
  // then update the ExecStore in alias cache before updating it
  cpu->ExeAliasCache->CommitStore(head_inst, tc);

  Process *p = tc->getProcessPtr();
  Addr vpn = p->pTable->pageAlign(head_inst->effAddr);
  if (!cpu->dtb->lookupAndUpdateEntry(vpn, true)){
    warn("No Entry found for commited store!\n");
  }


}

// here dont commit stack aliases!
// this is a function just for squashing a single instruction!
// if equal is true then we need to delete the entry because it's squashed
template<class Impl>
void
DefaultCommit<Impl>::squashExecuteAliasTable(DynInstPtr &inst)
{

    ThreadContext * tc = cpu->tcBase(inst->threadNumber);
    if (tc->enableCapability ){
        cpu->ExeAliasCache->SquashEntry(inst->seqNum);
    }
}

template <class Impl>
void
DefaultCommit<Impl>::getInsts()
{
    DPRINTF(Commit, "Getting instructions from Rename stage.\n");

    // Read any renamed instructions and place them into the ROB.
    int insts_to_process = std::min((int)renameWidth, fromRename->size);

    for (int inst_num = 0; inst_num < insts_to_process; ++inst_num) {
        DynInstPtr inst;

        inst = fromRename->insts[inst_num];
        ThreadID tid = inst->threadNumber;

        if (!inst->isSquashed() &&
            commitStatus[tid] != ROBSquashing &&
            commitStatus[tid] != TrapPending) {
            changedROBNumEntries[tid] = true;

            DPRINTF(Commit, "Inserting PC %s [sn:%i] [tid:%i] into ROB.\n",
                    inst->pcState(), inst->seqNum, tid);

            rob->insertInst(inst);

            assert(rob->getThreadEntries(tid) <= rob->getMaxEntries(tid));

            youngestSeqNum[tid] = inst->seqNum;
        } else {
            squashExecuteAliasTable(inst);
            DPRINTF(Commit, "Instruction PC %s [sn:%i] [tid:%i] was "
                    "squashed, skipping.\n",
                    inst->pcState(), inst->seqNum, tid);

        }
    }
}






template <class Impl>
void
DefaultCommit<Impl>::markCompletedInsts()
{
    // Grab completed insts out of the IEW instruction queue, and mark
    // instructions completed within the ROB.
    for (int inst_num = 0; inst_num < fromIEW->size; ++inst_num) {
        assert(fromIEW->insts[inst_num]);
        if (!fromIEW->insts[inst_num]->isSquashed() &&
            !fromIEW->insts[inst_num]->deferredDueToAliasCacheMiss()) {
            DPRINTF(Commit, "[tid:%i]: Marking PC %s, [sn:%lli] ready "
                    "within ROB.\n",
                    fromIEW->insts[inst_num]->threadNumber,
                    fromIEW->insts[inst_num]->pcState(),
                    fromIEW->insts[inst_num]->seqNum);

            // Mark the instruction as ready to commit.
            fromIEW->insts[inst_num]->setCanCommit();
        }
    }
}

template <class Impl>
void
DefaultCommit<Impl>::updateComInstStats(DynInstPtr &inst)
{
    ThreadID tid = inst->threadNumber;

    if (!inst->isMicroop() || inst->isLastMicroop())
        instsCommitted[tid]++;
    opsCommitted[tid]++;

    // To match the old model, don't count nops and instruction
    // prefetches towards the total commit count.
    if (!inst->isMicroopInjected() && !inst->isNop() && !inst->isInstPrefetch()) {
        cpu->instDone(tid, inst);
    }

    //
    //  Control Instructions
    //
    if (inst->isControl())
        statComBranches[tid]++;

    //
    //  Memory references
    //
    if (inst->isMemRef()) {
        statComRefs[tid]++;

        if (inst->isLoad()) {
            statComLoads[tid]++;
        }
    }

    if (inst->isMemBarrier()) {
        statComMembars[tid]++;
    }

    // Integer Instruction
    if (inst->isInteger())
        statComInteger[tid]++;

    // Floating Point Instruction
    if (inst->isFloating())
        statComFloating[tid]++;
    // Vector Instruction
    if (inst->isVector())
        statComVector[tid]++;

    // Function Calls
    if (inst->isCall())
        statComFunctionCalls[tid]++;

}

////////////////////////////////////////
//                                    //
//  SMT COMMIT POLICY MAINTAINED HERE //
//                                    //
////////////////////////////////////////
template <class Impl>
ThreadID
DefaultCommit<Impl>::getCommittingThread()
{
    if (numThreads > 1) {
        switch (commitPolicy) {

          case Aggressive:
            //If Policy is Aggressive, commit will call
            //this function multiple times per
            //cycle
            return oldestReady();

          case RoundRobin:
            return roundRobin();

          case OldestReady:
            return oldestReady();

          default:
            return InvalidThreadID;
        }
    } else {
        assert(!activeThreads->empty());
        ThreadID tid = activeThreads->front();

        if (commitStatus[tid] == Running ||
            commitStatus[tid] == Idle ||
            commitStatus[tid] == FetchTrapPending) {
            return tid;
        } else {
            return InvalidThreadID;
        }
    }
}

template<class Impl>
ThreadID
DefaultCommit<Impl>::roundRobin()
{
    list<ThreadID>::iterator pri_iter = priority_list.begin();
    list<ThreadID>::iterator end      = priority_list.end();

    while (pri_iter != end) {
        ThreadID tid = *pri_iter;

        if (commitStatus[tid] == Running ||
            commitStatus[tid] == Idle ||
            commitStatus[tid] == FetchTrapPending) {

            if (rob->isHeadReady(tid)) {
                priority_list.erase(pri_iter);
                priority_list.push_back(tid);

                return tid;
            }
        }

        pri_iter++;
    }

    return InvalidThreadID;
}

template<class Impl>
ThreadID
DefaultCommit<Impl>::oldestReady()
{
    unsigned oldest = 0;
    bool first = true;

    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (!rob->isEmpty(tid) &&
            (commitStatus[tid] == Running ||
             commitStatus[tid] == Idle ||
             commitStatus[tid] == FetchTrapPending)) {

            if (rob->isHeadReady(tid)) {

                DynInstPtr head_inst = rob->readHeadInst(tid);

                if (first) {
                    oldest = tid;
                    first = false;
                } else if (head_inst->seqNum < oldest) {
                    oldest = tid;
                }
            }
        }
    }

    if (!first) {
        return oldest;
    } else {
        return InvalidThreadID;
    }
}

template<class Impl>
Addr
DefaultCommit<Impl>::findCurrentStackTop(DynInstPtr& head_inst)
{

    for (size_t i = 0; i < head_inst->staticInst->numDestRegs(); i++) 
    {
        if (head_inst->destRegIdx(i).isIntReg())
        {
            // X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)inst->staticInst.get();
            // uint16_t dest_reg_idx = x86_inst->getUnflattenRegIndex(inst->staticInst->destRegIdx(i)); 
            
            uint16_t dest_reg_idx = head_inst->staticInst->destRegIdx(i).index();
            if (dest_reg_idx != X86ISA::INTREG_RSP) continue;
            assert(head_inst->numIntDestRegs() == 1 && "findCurrentStackTop:: Invalid number of dest regs!\n");
            // read the data
            uint64_t  dataRegContent = head_inst->readDestReg(head_inst->staticInst.get(), 0); 
            
            DPRINTF(AliasCache, "findCurrentStackTop: [SeqNum = %d] Stack Top: %x\n",
                    head_inst->seqNum, dataRegContent);
            
            return dataRegContent;


        }
    }

    return 0;
}


template<class Impl>
void
DefaultCommit<Impl>::updateFunctionActivationRecord(DynInstPtr& head_inst)
{
    assert((head_inst->isLoad() || head_inst->isStore()) && "head_inst is not a load/store instruction\n");
    if (head_inst->isMicroopInjected()) return;

    std::pair<Addr, Addr> startAndSize = cpu->thread[0]->activationRecords.top()->getFunctionObject().getFunctionStartAddressAndSize();
    Addr startAddr = startAndSize.first;
    Addr endAddr = startAndSize.first + startAndSize.second;
    assert(head_inst->pcState().instAddr() >= startAddr && head_inst->pcState().instAddr() <= endAddr  && "");
    for (size_t i = 0; i < head_inst->staticInst->numSrcRegs(); i++) 
    {
        if (head_inst->srcRegIdx(i).isIntReg())
        {
            
            uint16_t dest_reg_idx = head_inst->staticInst->srcRegIdx(i).index();
            if (dest_reg_idx != X86ISA::INTREG_RSP) continue;
            //assert(head_inst->numIntDestRegs() == 1 && "findCurrentStackTop:: Invalid number of dest regs!\n");
            // read the data
            //uint64_t  dataRegContent = head_inst->readDestReg(head_inst->staticInst.get(), 0); 
            
            DPRINTF(TrackFunctionObject, "updateFunctionActivationRecord: [SeqNum = %d] Stack Address: %x\n",
                    head_inst->seqNum, head_inst->effAddr);
            
            break;


        }
    }

}

#endif//__CPU_O3_COMMIT_IMPL_HH__
