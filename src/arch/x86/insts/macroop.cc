/*
 * macroop.cc
 *
 * Created on: May 11, 2018
 * Author: rasool
 */

 #include "macroop.hh"

 #include <iostream>

 #include "arch/x86/faults.hh"
 #include "arch/x86/generated/decoder.hh"
 #include "arch/x86/isa_traits.hh"
 #include "cpu/reg_class.hh"
 #include "enums/OpClass.hh"
 #include "debug/X86.hh"
 #include "debug/TypeTracker.hh"



namespace X86ISA
{

bool MacroopBase::filterInst(ThreadContext * tc, TheISA::PCState &nextPC) {

    Block fake;
    fake.payload = (Addr)nextPC.pc();
    fake.req_szB = 1;
    UWord foundkey = 1;
    UWord foundval = 1;
    unsigned char found = VG_lookupFM(tc->FunctionSymbols,
                                    &foundkey, &foundval, (UWord)&fake );
    if (found)
    {
      return true;
    }
    else
    {
      return false;
    }
}

void MacroopBase::updatePointerTracker(ThreadContext * tc, PCState &nextPC)
{
        assert(0);
      #define ENABLE_POINTER_TRACKER_DEBUG 0
      // this is probably a a little late but still can be effective
      if (!filterInst(tc,nextPC)) return;
      if (isInjected)             return;
      // its like we are executing microps here
      for (size_t i = 0; i < numMicroops; i++) {

           if (microops[i]->isLoad())
           {
                int base = microops[i]->getBase();
                if ((base < X86ISA::NumIntRegs))
                {


                }
           }
           else if (microops[i]->isStore())
           {
               int base = microops[i]->getBase();
               if ((base < X86ISA::NumIntRegs))
               {

               }
           }


       }

}


TheISA::PointerID MacroopBase::injectCheckMicroops(
        std::array<TheISA::PointerID, TheISA::NumIntRegs> _fetchArchRegsPid)
{

  
  if ((numMicroops > 0) && (!isInjected))
  {

    //   for (int j = 0; j < numMicroops; ++j)
    //   {   // do not inject into macroops which have control microops
    //       // TODO: function pointers have these kind of microops
    //       if (microops[j]->isControl() ||
    //           microops[j]->isDirectCtrl() ||
    //           microops[j]->isIndirectCtrl() ||
    //           microops[j]->isCondCtrl() ||
    //           microops[j]->isUncondCtrl() ||
    //           microops[j]->isMicroBranch())
    //       {
    //           return false;
    //       }
    //   }

      int i;
      for (i = 0; i < numMicroops; ++i)
      {

          // for all the load and stores inject check microop,
          if (microops[i]->isLoad() || microops[i]->isStore())
          {
              // we can filter explicit load/stores to stack
              // there are some implicit stack loads/stores which we don't want
            //   if (microops[i]->getBase() == X86ISA::INTREG_RSP)
            //   {
            //     continue;
            //   }
            //   else {

                X86ISA::X86StaticInst * x86_inst = (X86ISA::X86StaticInst *)microops[i].get();

                uint16_t src1 = x86_inst->getUnflattenRegIndex(microops[i]->srcRegIdx(1)); //base

                if (_fetchArchRegsPid[src1] != TheISA::PointerID(0))
                {
                    DPRINTF(TypeTracker, "Found a Store/Load that requires injection! BaseRegPid[%s]=%s\n",
                           TheISA::IntRegIndexStr(src1) , _fetchArchRegsPid[src1]);
                    for (int i = 0; i < TheISA::NumIntRegs; i++)
                    {
                        DPRINTF(TypeTracker, "fetchArchRegsPid[%s] = %s\n", TheISA::IntRegIndexStr(i), _fetchArchRegsPid[i]);
                    }
                    
                    return _fetchArchRegsPid[src1];
                }
                

                continue;
            //   }
          }
      }

      if (i >= numMicroops) {
        return TheISA::PointerID(0);
      }

  }
  return TheISA::PointerID(0);

}

void MacroopBase::deleteMicroOps(){
    if (numMicroops > 0){
        delete [] microops;
        microops = NULL;
        numMicroops = 0;
    }
}


void MacroopBase::undoInjecttion(){
    if (numMicroops > 0 &&  isInjected){

        isInjected = false;

        panic_if(numOfOriginalMicroops >= numMicroops,
                "numOfOriginalMicroops >= numMicroops");

        StaticInstPtr * microopTemp = new StaticInstPtr[numOfOriginalMicroops];
        int numOfInjectedUops = numMicroops - numOfOriginalMicroops;
        for (int i=0; i < numOfOriginalMicroops; i++)
            microopTemp[i] = microops[i + numOfInjectedUops];

        delete [] microops;
        microops = microopTemp;
        microops[0]->setFirstMicroop();
        numMicroops = numOfOriginalMicroops;

    }
    else if (numMicroops <= 0){
        //DPRINTF(Capability, "INVALID NUMBER OF MICROOPS\n");
        panic("Invalid  Number Of Microops");
    }

}


void
MacroopBase::injectBoundsCheck(PCState &nextPC, TheISA::PointerID _pid){

    // if there is a bounds check microop then no need to inject again
    // as in the IEW we will igonore it if it wasnt necessary
    panic_if(isInjected, "Injecting to an injected macroop!");

    if (numMicroops > 0 && !isInjected)
    {

        // remember to set and clear last micro of original microops
        for (int idx = 0; idx < numMicroops; ++idx)
        {
            if (microops[idx]->isLoad())
            {

              microops[0]->clearFirstMicroop();

              StaticInstPtr * microopTemp =
                                  new StaticInstPtr[numMicroops + 1];

              for (int i=0; i < numMicroops; i++)
                  microopTemp[i+1] = microops[i];

              StaticInstPtr micro_0 = (microops[idx]->getDataSize() >= 4) ?
                      (StaticInstPtr)(new X86ISAInst::LdBig(machInst,
                        "AP_BOUNDS_CHECK_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsFirstMicroop) |
                        (1ULL << StaticInst::IsMicroopInjected)|
                        (1ULL << StaticInst::IsBoundsCheckMicroop),
                        microops[idx]->getScale(),
                        InstRegIndex(microops[idx]->getIndex()),
                        InstRegIndex(microops[idx]->getBase()),
                        microops[idx]->getDisp(),
                        InstRegIndex(microops[idx]->getSegment()),
                        InstRegIndex(microops[idx]->getBase()),
                        microops[idx]->getDataSize(),
                        microops[idx]->getAddressSize(),
                        0)) :
                    (StaticInstPtr)(new X86ISAInst::Ld(machInst,
                        "AP_BOUNDS_CHECK_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsFirstMicroop) |
                        (1ULL << StaticInst::IsMicroopInjected)|
                        (1ULL << StaticInst::IsBoundsCheckMicroop),
                        microops[idx]->getScale(),
                        InstRegIndex(microops[idx]->getIndex()),
                        InstRegIndex(microops[idx]->getBase()),
                        microops[idx]->getDisp(),
                        InstRegIndex(microops[idx]->getSegment()),
                        InstRegIndex(microops[idx]->getBase()),
                        microops[idx]->getDataSize(),
                        microops[idx]->getAddressSize(),
                        0)
                        );
              
              micro_0->setStaticPointerID(_pid);
              microopTemp[0] = micro_0;

              delete [] microops;
              microops = microopTemp;
              numMicroops = numMicroops + 1;
              isInjected = true;
              break;
            }

            else if (microops[idx]->isStore())
            {

                microops[0]->clearFirstMicroop();

                StaticInstPtr * microopTemp =
                                    new StaticInstPtr[numMicroops + 1];

                for (int i=0; i < numMicroops; i++)
                    microopTemp[i+1] = microops[i];

                StaticInstPtr micro_0 = (microops[idx]->getDataSize() >= 4) ?
                            (StaticInstPtr)(new X86ISAInst::LdBig(machInst,
                              "AP_BOUNDS_CHECK_INJECT",
                              (1ULL << StaticInst::IsMicroop) |
                              (1ULL << StaticInst::IsFirstMicroop) |
                              (1ULL << StaticInst::IsMicroopInjected)|
                              (1ULL << StaticInst::IsBoundsCheckMicroop),
                              microops[idx]->getScale(),
                              InstRegIndex(microops[idx]->getIndex()),
                              InstRegIndex(microops[idx]->getBase()),
                              microops[idx]->getDisp(),
                              InstRegIndex(microops[idx]->getSegment()),
                              InstRegIndex(microops[idx]->getBase()),
                              microops[idx]->getDataSize(),
                              microops[idx]->getAddressSize(),
                              0)) :
                          (StaticInstPtr)(new X86ISAInst::Ld(machInst,
                              "AP_BOUNDS_CHECK_INJECT",
                              (1ULL << StaticInst::IsMicroop) |
                              (1ULL << StaticInst::IsFirstMicroop) |
                              (1ULL << StaticInst::IsMicroopInjected)|
                              (1ULL << StaticInst::IsBoundsCheckMicroop),
                              microops[idx]->getScale(),
                              InstRegIndex(microops[idx]->getIndex()),
                              InstRegIndex(microops[idx]->getBase()),
                              microops[idx]->getDisp(),
                              InstRegIndex(microops[idx]->getSegment()),
                              InstRegIndex(microops[idx]->getBase()),
                              microops[idx]->getDataSize(),
                              microops[idx]->getAddressSize(),
                              0)
                              );
                micro_0->setStaticPointerID(_pid);
                microopTemp[0] = micro_0;

                delete [] microops;
                microops = microopTemp;
                numMicroops = numMicroops + 1;
                isInjected = true;
                break;
            }

        }

        if (!isInjected)
            panic("wrong injection!");

    }

    else if (numMicroops <= 0)
    {
        panic("Invalid  Number Of Microops");
    }

 }

void
MacroopBase::injectMicroops(ThreadContext * _tc,
                            PCState &nextPC, 
                            TheISA::TyCHEAllocationPoint _sym,
                            TheISA::PointerID _pid)
{


    if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_MALLOC_BASE_COLLECT && 
        _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::MALLOC_SIZE)
    {
        injectAPMallocBaseCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_MALLOC_SIZE_COLLECT &&
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::NONE)
    {
        injectAPMallocSizeCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_FREE_CALL && 
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::NONE)
    {
        injectAPFreeCall(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_FREE_RET && 
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::FREE_CALL)
    {   
        injectAPFreeRet(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_BOUNDS_INJECT){
        injectBoundsCheck(nextPC, _pid);
      }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_CALLOC_BASE_COLLECT && 
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::CALLOC_SIZE)
    {
        injectAPCallocBaseCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_CALLOC_SIZE_COLLECT &&
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::NONE)
    {
        injectAPCallocSizeCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_REALLOC_BASE_COLLECT && 
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::REALLOC_SIZE)
    {
        injectAPReallocBaseCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_REALLOC_SIZE_COLLECT && 
            _tc->forntend_collector_status == ThreadContext::COLLECTOR_STATUS::NONE)
    {
        injectAPReallocSizeCollector(_tc, nextPC);
    }
    else if (_sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_IGONRE ||
            _sym.GetCheckType() == TheISA::TyCHEAllocationPoint::CheckType::AP_INVALID )
    {
        assert(false || "Ivalid inject type is detected!\n");
    }

}

void
MacroopBase::injectAPFreeRet(ThreadContext * _tc, PCState &nextPC){

  if (numMicroops > 0 && !isInjected){

          isInjected = true;

          // remember to set and clear last micro of original microops
          microops[0]->clearFirstMicroop();


          StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 1];

          for (int i=0; i < numMicroops; i++)
              microopTemp[i+1] = microops[i];

          StaticInstPtr micro_0 = new X86ISAInst::Mov(
                          machInst,
                          "AP_FREE_RET_INJECT",
                          (1ULL << StaticInst::IsMicroop) |
                          (1ULL << StaticInst::IsFirstMicroop) |
                          (1ULL << StaticInst::IsMicroopInjected) |
                          //(1ULL << StaticInst::IsSerializing)|
                          //(1ULL << StaticInst::IsSerializeBefore)|
                          (1ULL << StaticInst::IsFreeRetMicroop),
                          InstRegIndex(X86ISA::INTREG_RDI),
                          InstRegIndex(X86ISA::INTREG_RDI),
                          InstRegIndex(X86ISA::INTREG_RDI),
                          8,
                          0);


          microopTemp[0]   = micro_0;

          delete [] microops;
          microops = microopTemp;
          numMicroops = numMicroops + 1;




      }
      else if (numMicroops <= 0){
          panic("Invalid  Number Of Microops");

      }


}

void
MacroopBase::injectAPFreeCall(ThreadContext * _tc, PCState &nextPC){

if (numMicroops > 0 && !isInjected){

        isInjected = true;

        // remember to set and clear last micro of original microops
        microops[0]->clearFirstMicroop();

        DPRINTF(TypeTracker, "Injecting AP Free for PC address: %s\n", nextPC);
        StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 1];

        for (int i=0; i < numMicroops; i++)
            microopTemp[i+1] = microops[i];

        StaticInstPtr micro_0 = new X86ISAInst::Mov(
                        machInst,
                        "AP_FREE_CALL_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsFirstMicroop) |
                        (1ULL << StaticInst::IsMicroopInjected) |
                        //(1ULL << StaticInst::IsSerializing)|
                        //(1ULL << StaticInst::IsSerializeBefore)|
                        (1ULL << StaticInst::IsFreeCallMicroop),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        8,
                        0);


        microopTemp[0]   = micro_0;

        delete [] microops;
        microops = microopTemp;
        numMicroops = numMicroops + 1;




    }
    else if (numMicroops <= 0){
        panic("Invalid  Number Of Microops");

    }

}

void
MacroopBase::injectAPMallocSizeCollector(ThreadContext * _tc, PCState &nextPC){


    if (numMicroops > 0 && !isInjected){

        isInjected = true;

        // remember to set and clear last micro of original microops
        microops[0]->clearFirstMicroop();

        DPRINTF(TypeTracker, "Injecting AP Malloc Size Collector for PC address: %s\n", nextPC);
        StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

        for (int i=0; i < numMicroops; i++)
            microopTemp[i+2] = microops[i];

        StaticInstPtr micro_1 = new X86ISAInst::AddImmBig(
                        machInst,
                        "AP_MALLOC_SIZE_COLLECT_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsMicroopInjected),
                        //(1ULL << StaticInst::IsSerializing)|
                        //(1ULL << StaticInst::IsSerializeBefore),
                        InstRegIndex(X86ISA::INTREG_R16),
                        1,
                        InstRegIndex(X86ISA::INTREG_R16),
                        8,
                        0);

        StaticInstPtr micro_0 = new X86ISAInst::Mov(
                        machInst,
                        "AP_MALLOC_SIZE_COLLECT_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsMallocSizeCollectorMicroop)|
                        (1ULL << StaticInst::IsMicroopInjected)|
                        (1ULL << StaticInst::IsFirstMicroop),
                        //(1ULL << StaticInst::IsSerializing)|
                        //(1ULL << StaticInst::IsSerializeBefore),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        InstRegIndex(X86ISA::INTREG_RDI),
                        8,
                        0);



        microopTemp[0]   = micro_0;
        microopTemp[1]   = micro_1;

        delete [] microops;
        microops = microopTemp;
        numMicroops = numMicroops + 2;

    }
    else if (numMicroops <= 0){
        panic("Invalid  Number Of Microops");

    }

}


void
MacroopBase::injectAPMallocBaseCollector(ThreadContext * _tc, PCState &nextPC){

    if (numMicroops > 0 && !isInjected){

        isInjected = true;

        // remember to set and clear last micro of original microops
        microops[0]->clearFirstMicroop();

        DPRINTF(TypeTracker, "Injecting AP Malloc Base Collector for PC address: %s\n", nextPC);

        StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

        for (int i=0; i < numMicroops; i++)
            microopTemp[i+2] = microops[i];

        StaticInstPtr micro_0 = new X86ISAInst::Mov(
                              machInst,
                              "AP_MALLOC_BASE_COLLECT_INJECT",
                              (1ULL << StaticInst::IsMicroop) |
                              (1ULL << StaticInst::IsFirstMicroop) |
                              (1ULL << StaticInst::IsMicroopInjected),
                              //(1ULL << StaticInst::IsSerializing)|
                              //(1ULL << StaticInst::IsSerializeBefore),
                              InstRegIndex(X86ISA::INTREG_R16),
                              InstRegIndex(X86ISA::INTREG_R16),
                              InstRegIndex(X86ISA::INTREG_R16),
                              8,
                              0);

        StaticInstPtr micro_1 = new X86ISAInst::Mov(
                        machInst,
                        "AP_MALLOC_BASE_COLLECT_INJECT",
                        (1ULL << StaticInst::IsMicroop) |
                        (1ULL << StaticInst::IsMicroopInjected)|
                        (1ULL << StaticInst::IsMallocBaseCollectorMicroop),
                        //(1ULL << StaticInst::IsSerializing)|
                        //(1ULL << StaticInst::IsSerializeBefore),
                        InstRegIndex(X86ISA::INTREG_RAX),
                        InstRegIndex(X86ISA::INTREG_RAX),
                        InstRegIndex(X86ISA::INTREG_RAX),
                        8,
                        0);

        microopTemp[0] = micro_0;
        microopTemp[1] = micro_1;

        delete [] microops;
        microops = microopTemp;
        numMicroops = numMicroops + 2;

    }
    else if (numMicroops <= 0){
        panic("Invalid  Number Of Microops");
    }
}




 void
 MacroopBase::injectAPCallocSizeCollector(ThreadContext * _tc, PCState &nextPC)
 {

   if (numMicroops > 0 && !isInjected){

       isInjected = true;

       // remember to set and clear last micro of original microops
       microops[0]->clearFirstMicroop();
       
       DPRINTF(TypeTracker, "Injecting AP Calloc Size Collector for PC address: %s\n", nextPC);

       StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

       for (int i=0; i < numMicroops; i++)
           microopTemp[i+2] = microops[i];

       StaticInstPtr micro_1 = new X86ISAInst::AddImmBig(
                       machInst,
                       "AP_CALLOC_SIZE_COLLECT_INJECT",
                       (1ULL << StaticInst::IsMicroop) |
                       (1ULL << StaticInst::IsMicroopInjected),
                       //(1ULL << StaticInst::IsSerializing)|
                       //(1ULL << StaticInst::IsSerializeBefore),
                       InstRegIndex(X86ISA::INTREG_R16),
                       1,
                       InstRegIndex(X86ISA::INTREG_R16),
                       8,
                       0);

       StaticInstPtr micro_0 = new X86ISAInst::Mov(
                       machInst,
                       "AP_CALLOC_SIZE_COLLECT_INJECT",
                       (1ULL << StaticInst::IsMicroop) |
                       (1ULL << StaticInst::IsCallocSizeCollectorMicroop)|
                       (1ULL << StaticInst::IsMicroopInjected)|
                       (1ULL << StaticInst::IsFirstMicroop),
                       //(1ULL << StaticInst::IsSerializing)|
                       //(1ULL << StaticInst::IsSerializeBefore),
                       InstRegIndex(X86ISA::INTREG_RSI),
                       InstRegIndex(X86ISA::INTREG_RDI),
                       InstRegIndex(X86ISA::INTREG_RDI),
                       8,
                       0);



       microopTemp[0]   = micro_0;
       microopTemp[1]   = micro_1;

       delete [] microops;
       microops = microopTemp;
       numMicroops = numMicroops + 2;

   }
   else if (numMicroops <= 0){
       panic("Invalid  Number Of Microops");

   }


 }


 void
 MacroopBase::injectAPCallocBaseCollector(ThreadContext * _tc, PCState &nextPC)
 {

    if (numMicroops > 0 && !isInjected){

           isInjected = true;

           // remember to set and clear last micro of original microops
           microops[0]->clearFirstMicroop();


           StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

           for (int i=0; i < numMicroops; i++)
               microopTemp[i+2] = microops[i];

           StaticInstPtr micro_0 = new X86ISAInst::Mov(
                                 machInst,
                                 "AP_CALLOC_BASE_COLLECT_INJECT",
                                 (1ULL << StaticInst::IsMicroop) |
                                 (1ULL << StaticInst::IsFirstMicroop) |
                                 (1ULL << StaticInst::IsMicroopInjected),
                                 //(1ULL << StaticInst::IsSerializing)|
                                 //(1ULL << StaticInst::IsSerializeBefore),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 8,
                                 0);

           StaticInstPtr micro_1 = new X86ISAInst::Mov(
                           machInst,
                           "AP_CALLOC_BASE_COLLECT_INJECT",
                           (1ULL << StaticInst::IsMicroop) |
                           (1ULL << StaticInst::IsMicroopInjected)|
                           (1ULL << StaticInst::IsCallocBaseCollectorMicroop),
                           //(1ULL << StaticInst::IsSerializing)|
                           //(1ULL << StaticInst::IsSerializeBefore),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           8,
                           0);

           microopTemp[0] = micro_0;
           microopTemp[1] = micro_1;

           delete [] microops;
           microops = microopTemp;
           numMicroops = numMicroops + 2;

   }
   else if (numMicroops <= 0){
           panic("Invalid  Number Of Microops");
   }

 }


 void
 MacroopBase::injectAPReallocSizeCollector(ThreadContext * _tc,
                                          PCState &nextPC)
{


     if (numMicroops > 0 && !isInjected){

         isInjected = true;

         // remember to set and clear last micro of original microops
         microops[0]->clearFirstMicroop();


         StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

         for (int i=0; i < numMicroops; i++)
             microopTemp[i+2] = microops[i];

         StaticInstPtr micro_1 = new X86ISAInst::AddImmBig(
                         machInst,
                         "AP_REALLOC_SIZE_COLLECT_INJECT",
                         (1ULL << StaticInst::IsMicroop) |
                         (1ULL << StaticInst::IsMicroopInjected),
                         //(1ULL << StaticInst::IsSerializing)|
                         //(1ULL << StaticInst::IsSerializeBefore),
                         InstRegIndex(X86ISA::INTREG_R16),
                         1,
                         InstRegIndex(X86ISA::INTREG_R16),
                         8,
                         0);

         StaticInstPtr micro_0 = new X86ISAInst::Mov(
                         machInst,
                         "AP_REALLOC_SIZE_COLLECT_INJECT",
                         (1ULL << StaticInst::IsMicroop) |
                         (1ULL << StaticInst::IsReallocSizeCollectorMicroop)|
                         (1ULL << StaticInst::IsMicroopInjected)|
                         (1ULL << StaticInst::IsFirstMicroop),
                         //(1ULL << StaticInst::IsSerializing)|
                         //(1ULL << StaticInst::IsSerializeBefore),
                         InstRegIndex(X86ISA::INTREG_RSI),
                         InstRegIndex(X86ISA::INTREG_RDI),
                         InstRegIndex(X86ISA::INTREG_RDI),
                         8,
                         0);



         microopTemp[0]   = micro_0;
         microopTemp[1]   = micro_1;

         delete [] microops;
         microops = microopTemp;
         numMicroops = numMicroops + 2;

     }
     else if (numMicroops <= 0){
         panic("Invalid  Number Of Microops");

     }



 }


 void
 MacroopBase::injectAPReallocBaseCollector(ThreadContext * _tc,
                                          PCState &nextPC)
{

    if (numMicroops > 0 && !isInjected){

           isInjected = true;

           // remember to set and clear last micro of original microops
           microops[0]->clearFirstMicroop();


           StaticInstPtr * microopTemp = new StaticInstPtr[numMicroops + 2];

           for (int i=0; i < numMicroops; i++)
               microopTemp[i+2] = microops[i];

           StaticInstPtr micro_0 = new X86ISAInst::Mov(
                                 machInst,
                                 "AP_REALLOC_BASE_COLLECT_INJECT",
                                 (1ULL << StaticInst::IsMicroop) |
                                 (1ULL << StaticInst::IsFirstMicroop) |
                                 (1ULL << StaticInst::IsMicroopInjected),
                                 //(1ULL << StaticInst::IsSerializing)|
                                 //(1ULL << StaticInst::IsSerializeBefore),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 InstRegIndex(X86ISA::INTREG_R16),
                                 8,
                                 0);

           StaticInstPtr micro_1 = new X86ISAInst::Mov(
                           machInst,
                           "AP_REALLOC_BASE_COLLECT_INJECT",
                           (1ULL << StaticInst::IsMicroop) |
                           (1ULL << StaticInst::IsMicroopInjected)| (1ULL << StaticInst::IsReallocBaseCollectorMicroop) ,
                           //(1ULL << StaticInst::IsSerializing)|
                           //(1ULL << StaticInst::IsSerializeBefore),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           InstRegIndex(X86ISA::INTREG_RAX),
                           8,
                           0);

           microopTemp[0] = micro_0;
           microopTemp[1] = micro_1;

           delete [] microops;
           microops = microopTemp;
           numMicroops = numMicroops + 2;

   }
   else if (numMicroops <= 0){
           panic("Invalid  Number Of Microops");
   }

}


}
