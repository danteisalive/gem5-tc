/*
 * Copyright (c) 2007 The Hewlett-Packard Development Company
 * All rights reserved.
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
 * Authors: Gabe Black
 */

#ifndef __ARCH_X86_INSTS_MACROOP_HH__
#define __ARCH_X86_INSTS_MACROOP_HH__

#include "arch/x86/emulenv.hh"
#include "arch/x86/insts/badmicroop.hh"
#include "arch/x86/insts/static_inst.hh"
#include "arch/x86/types.hh"
#include "debug/Capability.hh"
#include "mem/request.hh"


namespace X86ISA
{
//using namespace X86ISAInst;
// Base class for combinationally generated macroops
class MacroopBase : public X86StaticInst
{
public:

  protected:
     const char *macrocodeBlock;
     TheISA::PointerID macroop_pid{0};

     uint32_t numMicroops;
     uint32_t numOfOriginalMicroops;
     X86ISA::EmulEnv env;
    //Constructor.
    MacroopBase(const char *mnem, ExtMachInst _machInst,
            uint32_t _numMicroops, X86ISA::EmulEnv _env) :
                X86StaticInst(mnem, _machInst, No_OpClass),
                numMicroops(_numMicroops), env(_env)
    {
        assert(numMicroops);
        microops = new StaticInstPtr[numMicroops];
        flags[IsMacroop] = true;
        numOfOriginalMicroops = _numMicroops;
    }

    ~MacroopBase()
    {
        if (microops) {
            delete [] microops;
        }
    }

    StaticInstPtr * microops;

    StaticInstPtr
    fetchMicroop(MicroPC microPC) const
    {
        if (microPC >= numMicroops)
            return badMicroop;
        else
            return microops[microPC];
    }

    std::string
    generateDisassembly(Addr pc, const SymbolTable *symtab) const
    {
        return mnemonic;
    }

  public:

    uint64_t
    getNumOfMicroops()
    {
      return numMicroops;
    }

    ExtMachInst
    getExtMachInst()
    {
        return machInst;
    }

    X86ISA::EmulEnv
    getEmulEnv()
    {
        return env;
    }

    void setMacroopPid(TheISA::PointerID _pid){
       macroop_pid = _pid;
    }

    TheISA::PointerID getMacroopPid(){
      return macroop_pid;
    }

    StaticInstPtr *
    getMicroops() const
    {
        return microops;

    }
    const char *
    getMacroName(){

        if (macrocodeBlock != NULL)
            return macrocodeBlock;
        else
            return "NULL";

    }

    bool filterInst(ThreadContext * tc,PCState &nextPC);
    TheISA::PointerID injectCheckMicroops(
        std::array<TheISA::PointerID, TheISA::NumIntRegs> _fetchArchRegsPid);

    void updatePointerTracker(ThreadContext * tc, PCState &nextPC);

    void injectMicroops( ThreadContext * _tc, 
                         PCState &nextPC,
                         TheISA::TyCHEAllocationPoint _sym,
                         TheISA::PointerID _pid
                       );

    void injectAPMallocBaseCollector(ThreadContext * _tc,PCState &nextPC);
    void injectAPMallocSizeCollector(ThreadContext * _tc,PCState &nextPC);

    void injectAPCallocBaseCollector(ThreadContext * _tc,PCState &nextPC);
    void injectAPCallocSizeCollector(ThreadContext * _tc,PCState &nextPC);

    void injectAPReallocBaseCollector(ThreadContext * _tc,PCState &nextPC);
    void injectAPReallocSizeCollector(ThreadContext * _tc,PCState &nextPC);

    void injectAPFreeCall(ThreadContext * _tc,PCState &nextPC);
    void injectAPFreeRet(ThreadContext * _tc,PCState &nextPC);

    void injectBoundsCheck(PCState &nextPC, TheISA::PointerID _pid);
    void undoInjecttion();
    void deleteMicroOps();


};
}

#endif //__ARCH_X86_INSTS_MACROOP_HH__
