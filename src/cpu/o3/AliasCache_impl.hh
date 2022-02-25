/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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
 * Authors: Korey Sewell
 */

#include "cpu/o3/AliasCache.hh"
#include "debug/AliasCache.hh"



#define ENABLE_ALIAS_CACHE_DEBUG 0

template <class Impl>
LRUAliasCache<Impl>::LRUAliasCache(uint64_t _num_ways,
                            uint64_t _cache_block_size,
                            uint64_t _cache_size) :
            NumWays(_num_ways), CacheSize(_cache_size),
            CacheBlockSize(_cache_block_size),
            total_accesses(0), total_hits(0), total_misses(0),
            outstandingRead(0), outstandingWrite(0)
    {

                NumSets = (CacheSize / CacheBlockSize) / (NumWays);
                NumEntriesInCache = NumSets * NumWays;
                BitsPerSet = std::log2(NumSets);
                BitsPerBlock = std::log2(CacheBlockSize);
                ShiftAmount = BitsPerSet + BitsPerBlock;

                AliasCache = new TheISA::CacheEntry*[NumSets];
                for (size_t i = 0; i < NumSets; i++) {
                  AliasCache[i]  = new TheISA::CacheEntry[NumWays];
                }

                VictimCache = new LRUVictimCache<Impl>(32);


                for (size_t set = 0; set < NumSets; set++) {
                    for (size_t way = 0; way < NumWays; way++) {
                      AliasCache[set][way].tag = 0;
                      AliasCache[set][way].valid = false;
                      AliasCache[set][way].dirty = false;
                      AliasCache[set][way].lruAge = 1;
                      AliasCache[set][way].pid = TheISA::PointerID{0};
                      AliasCache[set][way].vaddr = 0;
                    }
                }

                stack_base = 0x7FFFFFFFF000ULL;
                max_stack_size = 32 * 1024 * 1024;
                next_thread_stack_base = stack_base - max_stack_size;
                RSPPrevValue = next_thread_stack_base;

    }
    template <class Impl>
    LRUAliasCache<Impl>::~LRUAliasCache(){
          delete [] AliasCache;
          delete VictimCache;
    }

        // this function is called when we want to write or read an alias
        // from the table. In the case of a write, we need to update the
        // sqn of the entry in the case of a squash
    template <class Impl>
    bool LRUAliasCache<Impl>::Access(DynInstPtr& inst, ThreadContext* tc, TheISA::PointerID* pid )
    {
            Addr vaddr = inst->effAddr;
            DPRINTF(AliasCache, " Access:: Access for EffAddr: 0x%x\n", vaddr);
            // first look into the SQ
            bool SQHit = AccessStoreQueue(inst, pid);
            if (SQHit){
              return true;
            }

            // if we are here it means a miss to SQ
            Addr thisIsTheTag = vaddr >> (ShiftAmount); //tag of the VA

            //Extract the set from the VA
            Addr thisIsTheSet =
                ((vaddr - (thisIsTheTag << ShiftAmount)) >> BitsPerBlock);


            assert(thisIsTheSet < NumSets);


            for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {

                if (AliasCache[thisIsTheSet][wayNum].valid &&
                    AliasCache[thisIsTheSet][wayNum].tag == thisIsTheTag)
                {

                    assert(AliasCache[thisIsTheSet][wayNum].vaddr);
                    assert(AliasCache[thisIsTheSet][wayNum].vaddr == vaddr);
                    AliasCache[thisIsTheSet][wayNum].valid = true;
                    *pid = AliasCache[thisIsTheSet][wayNum].pid;

                    // increase the lru age
                    for (size_t i = 0; i < NumWays; i++) {
                      AliasCache[thisIsTheSet][i].lruAge++;
                    }
                    AliasCache[thisIsTheSet][wayNum].lruAge = 0;

                    DPRINTF(AliasCache, "LRUAliasCache::Access::Found an alias in Alias Cache :: EffAddr: 0x%x PID=%s\n", 
                        AliasCache[thisIsTheSet][wayNum].vaddr, 
                        AliasCache[thisIsTheSet][wayNum].pid
                    );
                    return true;

                }

            }
            DPRINTF(AliasCache, "LRUAliasCache::Access::Miss in Alias Cache! Trying to find a new replacement for EffAddr: 0x%x\n", 
                    vaddr);
            // if we are here then it means a miss
            // find the candiate for replamcement
            size_t candidateWay = 0;
            size_t candiateLruAge = 0;
            for (int i = 0; i < NumWays; i++) {
                if (!AliasCache[thisIsTheSet][i].valid)
                {
                  candidateWay = i;
                  break;
                }
                else if (AliasCache[thisIsTheSet][i].lruAge > candiateLruAge)
                {
                  candiateLruAge = AliasCache[thisIsTheSet][i].lruAge;
                  candidateWay = i;
                }
            }

            DPRINTF(AliasCache, "LRUAliasCache::Access::Candidiate way for replacement: EffAddr: 0x%x Candidate Way=%d\n", 
                    vaddr,
                    candidateWay);

            for (size_t i = 0; i < NumWays; i++) {
              AliasCache[thisIsTheSet][i].lruAge++;
            }


            // if the candidate entry is valid just write it to the
            // victim cache no matter it is dirty or not
            if (AliasCache[thisIsTheSet][candidateWay].valid)
            {
                VictimCache->VictimCacheWriteBack(
                              AliasCache[thisIsTheSet][candidateWay].vaddr);
            }
            // new read it from shadow_memory
            // if the page does not have any pid then it's defenitly a
            // PID(0). In this case just send it back and do not update
            // alias cache as it will just polute the cache and deacrese the
            // hit rate. If there is a page for it update the cache in any case
            Process *p = tc->getProcessPtr();
            Addr vpn = p->pTable->pageAlign(vaddr); // find the vpn
            auto it_lv1 = tc->ShadowMemory.find(vpn);
            if (it_lv1 != tc->ShadowMemory.end() && it_lv1->second.size() != 0)
            {
                // if the replamcement candidate is dirty we need to
                // writeback it before replacing it with new one
                if (AliasCache[thisIsTheSet][candidateWay].valid &&
                    AliasCache[thisIsTheSet][candidateWay].dirty)
                {   
                    uint64_t wb_addr = AliasCache[thisIsTheSet][candidateWay].vaddr;
                    TheISA::PointerID wb_pid = AliasCache[thisIsTheSet][candidateWay].pid;
                    DPRINTF(AliasCache, "LRUAliasCache::Access::WriteBack for EffAddr: 0x%x in Shadow Memory! wb_pid=%s\n", 
                            vaddr,wb_pid);
                    //send the wb_addr to WbBuffer;
                    WriteBack(wb_addr);
                    //Commit it to the SM
                    CommitToShadowMemory(wb_addr, tc, wb_pid);
                }

                // the page is there and not empty
                auto it_lv2 = it_lv1->second.find(vaddr);
                if (it_lv2 != it_lv1->second.end()){
                  DPRINTF(AliasCache, "LRUAliasCache::Access::Page Found! Returning Alias for EffAddr: 0x%x pid=%s\n", 
                            vaddr,
                            it_lv2->second);
                  *pid = it_lv2->second;
                }
                else {
                  DPRINTF(AliasCache, "LRUAliasCache::Access::No Page Found! Returning Alias for EffAddr: 0x%x pid=%s\n", 
                            vaddr,
                            TheISA::PointerID(0));
                  *pid = TheISA::PointerID(0);
                }

                AliasCache[thisIsTheSet][candidateWay].valid = true;
                AliasCache[thisIsTheSet][candidateWay].pid = *pid;
                AliasCache[thisIsTheSet][candidateWay].dirty = false;
                AliasCache[thisIsTheSet][candidateWay].tag = thisIsTheTag;
                AliasCache[thisIsTheSet][candidateWay].lruAge = 0;
                AliasCache[thisIsTheSet][candidateWay].vaddr = vaddr;

            }
            else {
            // there is no alias in this page threfore just send back PID(0)
              DPRINTF(AliasCache, "LRUAliasCache::Access::Cannot find an alias for EffAddr: 0x%x in Shadow Memory!\n", 
                    vaddr);
                *pid = TheISA::PointerID(0);
            }

            return false;


    }

    // just initiates the access, in the case of miss
    // replacement hapeens after miss is handled
    // if it's a hit, there is no stall and InitiateAccess is complete
    template <class Impl>
    bool LRUAliasCache<Impl>::InitiateAccess(Addr vaddr, ThreadContext* tc){

        //TODO: stats
        total_accesses = total_accesses + 1;

        DPRINTF(AliasCache, " InitiateAccess::Initiating Access for EffAddr: 0x%x\n", vaddr);

        // first look into the SQ
        TheISA::PointerID* pid = new TheISA::PointerID(0);
        bool SQHit = AccessStoreQueue(vaddr, pid);
        if (SQHit){
            total_hits++;
            return true;
        }

        // if we are here it means a miss to SQ
        Addr thisIsTheTag = vaddr >> (ShiftAmount); //tag of the VA

        //Extract the set from the VA
        Addr thisIsTheSet =
                ((vaddr - (thisIsTheTag << ShiftAmount)) >> BitsPerBlock);


        assert(thisIsTheSet < NumSets);

        DumpAliasCache();
        for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {

            if (AliasCache[thisIsTheSet][wayNum].valid &&
                AliasCache[thisIsTheSet][wayNum].tag == thisIsTheTag)
            {

                total_hits++;
                assert(AliasCache[thisIsTheSet][wayNum].vaddr);
                assert(AliasCache[thisIsTheSet][wayNum].vaddr == vaddr);
                return true;

            }

        }

        if (VictimCache->VictimCacheInitiateRead(vaddr))
        {
          total_hits++;
          return true;
        }

        // if we are here then it means a miss
        // find the candiate for replamcement
        total_misses++;
        outstandingRead++;
        return false;


    }

    template <class Impl>
    bool LRUAliasCache<Impl>::CommitStore(DynInstPtr& head_inst, ThreadContext* tc)
    {
      // here commit the youngest entry of the ExeAliasBuffer to shadow memory
      // which is actually the CommitAliasTable
      Addr vaddr = head_inst->effAddr;
      uint64_t storeSeqNum = head_inst->seqNum;
      TheISA::PointerID writeback_pid = head_inst->dyn_pid;


      auto it = ExeAliasTableBuffer.begin();
      while(it != ExeAliasTableBuffer.end()) {

            if (it->first->seqNum == storeSeqNum &&
                it->first->effAddr == vaddr)
            {

                // this is not a stack alias therfore commit it to
                // shadow memory and then erase it
                //writeback to alias cache
                // if this page has no alias and wb_pid is zero
                // do not send it for commit just remove it
                Process *p = tc->getProcessPtr();
                Addr vaddr_vpn = p->pTable->pageAlign(vaddr);
                auto sm_it = tc->ShadowMemory.find(vaddr_vpn);

                bool commited = false;
                if (writeback_pid != TheISA::PointerID(0) ||
                    sm_it != tc->ShadowMemory.end())
                {
                  commited = Commit(it->first->effAddr, tc, writeback_pid);
                }
                //delete from alias store buffer
                it = ExeAliasTableBuffer.erase(it);
                DPRINTF(AliasCache, "LRUAliasCache::CommitStore:: %s Alias with VAddr=0x%x PID=%s to Shadow Memory!\n",
                        commited ? "COMMITED":"DID NOT COMMIT" ,it->first->effAddr, writeback_pid);
                DumpShadowMemory(tc);
                return true;
            }
            else 
            {
              ++it;
            }
      }

      // never should reach here!
      panic("LRUAliasCache::CommitStore::Commiting a store which cannot be found!\n");
      return false;
      
    }

    template <class Impl>
    bool LRUAliasCache<Impl>::Commit(Addr vaddr,ThreadContext* tc, TheISA::PointerID& pid)
    {
        // first tries to find the entry if it is a hit then updates
        // the entry and if it's not there first evicts an entry and
        // then overwrite it
        // if the dirty flags is set then we need an update to ShadowMemory
        //TODO: stats

        //TODO: stats
        Addr thisIsTheTag = vaddr >> (ShiftAmount); //tag of the VA

        //Extract the set from the VA
        Addr thisIsTheSet =
            ((vaddr - (thisIsTheTag << ShiftAmount)) >> BitsPerBlock);


        assert(thisIsTheSet < NumSets);


        for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {

            if (AliasCache[thisIsTheSet][wayNum].valid &&
                AliasCache[thisIsTheSet][wayNum].tag == thisIsTheTag)
            {

                assert(AliasCache[thisIsTheSet][wayNum].vaddr);
                // just update the entry and return!
                AliasCache[thisIsTheSet][wayNum].tag   = thisIsTheTag;
                AliasCache[thisIsTheSet][wayNum].valid = true;
                AliasCache[thisIsTheSet][wayNum].dirty = true;
                AliasCache[thisIsTheSet][wayNum].pid   = pid;
                AliasCache[thisIsTheSet][wayNum].vaddr = vaddr;

                DPRINTF(AliasCache, "LRUAliasCache::Commit::Found an Entry with the same Tag!\n");

                // increase the lru age
                for (size_t i = 0; i < NumWays; i++) {
                  AliasCache[thisIsTheSet][i].lruAge++;
                }

                AliasCache[thisIsTheSet][wayNum].lruAge = 0;

                CommitToShadowMemory(vaddr, tc, pid);
                return true;

            }

        }
        // if we are here then it means a miss
        // find the candiate for replamcement
        DPRINTF(AliasCache, "LRUAliasCache::Commit::Cannot Find an Entry with the same Tag! Trying to replace!\n");

        size_t candidateWay = 0;
        size_t candiateLruAge = 0;
        for (int i = 0; i < NumWays; i++) {
            if (!AliasCache[thisIsTheSet][i].valid){
              candidateWay = i;
              break;
            }
            else if (AliasCache[thisIsTheSet][i].lruAge > candiateLruAge){
              candiateLruAge = AliasCache[thisIsTheSet][i].lruAge;
              candidateWay = i;
            }
        }

        for (size_t i = 0; i < NumWays; i++) {
          AliasCache[thisIsTheSet][i].lruAge++;
        }
        
        DPRINTF(AliasCache, "LRUAliasCache::Commit::Way=%d with lruAge=%d is selected for replacement!\n", candidateWay, candiateLruAge);


        // This entry is going to get evicted no matter it's dirty or not
        // just put it into the victim cache
        if (AliasCache[thisIsTheSet][candidateWay].valid)
        {
            VictimCache->VictimCacheWriteBack(
                          AliasCache[thisIsTheSet][candidateWay].vaddr);
        }

        // here we know that the entry is not in the cache
        // writeback to ShadowMemory if the entry that is going to get
        // overwirtten is dirty
        if (AliasCache[thisIsTheSet][candidateWay].valid &&
            AliasCache[thisIsTheSet][candidateWay].dirty)
        {
            uint64_t wb_addr = AliasCache[thisIsTheSet][candidateWay].vaddr;
            TheISA::PointerID wb_pid = AliasCache[thisIsTheSet][candidateWay].pid;
            CommitToShadowMemory(wb_addr, tc, wb_pid);
        }

        // now overwrite
        AliasCache[thisIsTheSet][candidateWay].tag =thisIsTheTag;
        AliasCache[thisIsTheSet][candidateWay].valid = true;
        AliasCache[thisIsTheSet][candidateWay].dirty = true;
        AliasCache[thisIsTheSet][candidateWay].pid   = pid;
        AliasCache[thisIsTheSet][candidateWay].vaddr = vaddr;
        AliasCache[thisIsTheSet][candidateWay].lruAge = 0;

        // now commit it to the Shadow Memory
        CommitToShadowMemory(vaddr, tc, pid);

        return true;

    }
    template <class Impl>
    bool LRUAliasCache<Impl>::CommitToShadowMemory(Addr vaddr,ThreadContext* tc, TheISA::PointerID& pid)
    {
        Process *p = tc->getProcessPtr();
        Addr vpn = p->pTable->pageAlign(vaddr);
        if (pid != TheISA::PointerID(0)){
            DPRINTF(AliasCache, "LRUAliasCache::CommitToShadowMemory:: Commiting an Alias for vaddr=0x%x to Shadow Memory! PID=%s\n", 
                    vaddr, pid);
            tc->ShadowMemory[vpn][vaddr] = pid;
        }
        else {
            // if the pid == 0 we writeback if we can find the entry in
            // the Shadow Memory
            auto it_lv1 = tc->ShadowMemory.find(vpn);
            if (it_lv1 != tc->ShadowMemory.end() && it_lv1->second.size() != 0)
            {
                auto it_lv2 = it_lv1->second.find(vaddr);
                if (it_lv2 != it_lv1->second.end())
                {
                    DPRINTF(AliasCache, "LRUAliasCache::CommitToShadowMemory:: Found a Previous Entry and Commiting an Alias for vaddr=%d to Shadow Memory! PID=%s\n", 
                            vaddr, pid);
                    tc->ShadowMemory[vpn][vaddr] = pid;
                }
                DPRINTF(AliasCache, "LRUAliasCache::CommitToShadowMemory:: Cannot Find a Previous Alias Entry for vaddr=%d to Shadow Memory! PID=%s\n", 
                            vaddr, pid);
            }
        }

        return true;
    }
    template <class Impl>
    bool LRUAliasCache<Impl>::Invalidate( ThreadContext* tc, TheISA::PointerID& pid){

      // loop through the cache and invalid all freed entrys
      for (size_t setNum = 0; setNum < NumSets; setNum++) {
        for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {
          if (AliasCache[setNum][wayNum].valid &&
              AliasCache[setNum][wayNum].pid == pid)
          {

            DPRINTF(AliasCache, " Invalidate:: EffAddr: 0x%x PID=%s\n", 
                    AliasCache[setNum][wayNum].vaddr,
                    AliasCache[setNum][wayNum].pid
            );
            AliasCache[setNum][wayNum].valid = false;
          }
        }
      }

      // erase from ShadowMemory
      for (auto it_lv1 = tc->ShadowMemory.begin(),
                        next_it_lv1 = it_lv1;
                        it_lv1 != tc->ShadowMemory.end();
                        it_lv1 = next_it_lv1)
      {
              ++next_it_lv1;
              if (it_lv1->second.size() == 0)
              {
                  DPRINTF(AliasCache, " Invalidate:: Empty Alias Page! Erasing Page for EffAddr: 0x%x from Sahdow Memory!\n", 
                          it_lv1->first
                  );
                  tc->ShadowMemory.erase(it_lv1);
              }
              else {
                  for (auto it_lv2 = it_lv1->second.cbegin(),
                           next_it_lv2 = it_lv2;
                           it_lv2 != it_lv1->second.cend();
                           it_lv2 = next_it_lv2)
                  {
                      ++next_it_lv2;
                      if (it_lv2->second.GetPointerID() == pid.GetPointerID())
                      {
                        DPRINTF(AliasCache, " Invalidate:: Erasing EffAddr: 0x%x PID=%s from Sahdow Memory!\n", 
                                it_lv2->first, it_lv2->second
                        );
                        it_lv1->second.erase(it_lv2);
                      }
                  }
              }
        }

        // delete all aliases that match this pid from exe alias table store
        // buffer
        // commit alias table will be updates in its own collector
        // this removal shoidl happen when returnin from Free function
        // but as we dont know the pid at the return and we are not tracking
        // anything during the free fucntion we can safelydo it here!
        auto it = ExeAliasTableBuffer.begin();
        while(it != ExeAliasTableBuffer.end()) {

            if (it->second.GetPointerID() == pid.GetPointerID()) 
            {
              it = ExeAliasTableBuffer.erase(it);
            }
            else 
            {
              ++it;
            }
        }
        return true;


    }

    template <class Impl>
    bool LRUAliasCache<Impl>::RemoveStackAliases(Addr stack_addr, ThreadContext* tc){

        if (!(stack_addr <= stack_base &&
              stack_addr >= next_thread_stack_base))
            return false;

        Process *p = tc->getProcessPtr();
        Addr stack_vpn = p->pTable->pageAlign(stack_addr);
        DPRINTF(AliasCache, "RemoveStackAliases:: Current Stack Top: 0x%x Previous Stack Top:0x%x!\n", 
                      stack_addr, RSPPrevValue);
        if (stack_addr == RSPPrevValue) return false;

        auto it_lv1 = tc->ShadowMemory.find(stack_vpn);
        if (it_lv1 != tc->ShadowMemory.end() && it_lv1->second.size() != 0)
        {
           auto it_lv2 = it_lv1->second.lower_bound(stack_addr);
           if (it_lv2 != it_lv1->second.end())
           {
              DPRINTF(AliasCache, "RemoveStackAliases:: Erasing EffAddr: 0x%x PID=%s from Sahdow Memory!\n", 
                      it_lv2->first, it_lv2->second
              );
              it_lv1->second.erase(it_lv1->second.begin(), it_lv2);
           }
        }

        // loop through the cache and invalid all freed entrys
        for (size_t setNum = 0; setNum < NumSets; setNum++) {
          for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {
            if (AliasCache[setNum][wayNum].valid)
            {
              assert(AliasCache[setNum][wayNum].vaddr);
              if (AliasCache[setNum][wayNum].vaddr >= next_thread_stack_base &&
                  AliasCache[setNum][wayNum].vaddr < stack_addr)
              {
                  DPRINTF(AliasCache, "RemoveStackAliases:: Stack Top: 0x%x Invalidating ALias with EffAddr: 0x%x PID=%s\n", 
                    stack_addr, 
                    AliasCache[setNum][wayNum].vaddr,
                    AliasCache[setNum][wayNum].pid
                  );

                  AliasCache[setNum][wayNum].valid = false;
              }
            }
          }
        }

        RSPPrevValue = stack_addr;

        DumpAliasTableBuffer();
        DumpAliasCache();
        DumpShadowMemory(tc);

        return true;
    }

    template <class Impl>
    bool LRUAliasCache<Impl>::InsertStoreQueue(DynInstPtr& inst)
    {

      uint64_t seqNum =  inst->seqNum;
      Addr effAddr = inst->effAddr;
      TheISA::PointerID pid = inst->dyn_pid;
      DPRINTF(AliasCache, "Alias Cache InsertStoreQueue:: Inst: %x SeqNum: %d EffAddr: 0x%x\n", inst.get() , seqNum, effAddr);
      ExeAliasTableBuffer.push_back(std::make_pair(inst, pid));

      DumpAliasTableBuffer();

      return true;

    }
    template <class Impl>
    bool LRUAliasCache<Impl>::Squash(uint64_t squashed_num, bool include_inst){

      auto  exe_alias_buffer = ExeAliasTableBuffer.begin();
      while(exe_alias_buffer != ExeAliasTableBuffer.end()) {

          if (include_inst &&
              (exe_alias_buffer->first->seqNum >= squashed_num))
          {
            DPRINTF(AliasCache, "Alias Cache Squash (Include Inst):: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                    exe_alias_buffer->first->seqNum, 
                    exe_alias_buffer->first->effAddr,
                    exe_alias_buffer->second
                    );
            
            exe_alias_buffer = ExeAliasTableBuffer.erase(exe_alias_buffer);
          }
          else if (!include_inst &&
                  (exe_alias_buffer->first->seqNum > squashed_num))
          {
            DPRINTF(AliasCache, "Alias Cache Squash !(Include Inst):: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                    exe_alias_buffer->first->seqNum, 
                    exe_alias_buffer->first->effAddr,
                    exe_alias_buffer->second
                    );
            exe_alias_buffer = ExeAliasTableBuffer.erase(exe_alias_buffer);
          }
          
          else ++exe_alias_buffer;
      }


      DumpAliasTableBuffer();

      return true;
    }

    template <class Impl>
    bool LRUAliasCache<Impl>::AccessStoreQueue(Addr effAddr, TheISA::PointerID* pid)
    {
      DumpAliasTableBuffer();
      //first look in Execute Alias store buffer
      *pid = TheISA::PointerID(0);
      for (auto exe_alias_buffer = ExeAliasTableBuffer.rbegin();
                  exe_alias_buffer != ExeAliasTableBuffer.rend();
                      ++exe_alias_buffer)
      {
          if (exe_alias_buffer->first->effAddr == effAddr){
            DPRINTF(AliasCache, " AccessStoreQueue::Found an alias in Alias Cache Store Queue:: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                  exe_alias_buffer->first->seqNum, 
                  exe_alias_buffer->first->effAddr,
                  exe_alias_buffer->second
                  );
        
            *pid = exe_alias_buffer->second;
            return true;  // found in SQ
          }

       }

        DPRINTF(AliasCache, " AccessStoreQueue::Cannot Find an alias in Alias Cache Store Queue for EffAddr: 0x%x\n", 
                effAddr);
       return false; // not in SQ
    }



    template <class Impl>
    bool LRUAliasCache<Impl>::AccessStoreQueue(DynInstPtr& inst, TheISA::PointerID* pid)
    {
      Addr effAddr = inst->effAddr;

      DumpAliasTableBuffer();
      //first look in Execute Alias store buffer
      *pid = TheISA::PointerID(0);
      for (auto exe_alias_buffer = ExeAliasTableBuffer.rbegin();
                  exe_alias_buffer != ExeAliasTableBuffer.rend();
                      ++exe_alias_buffer)
      {
          if (exe_alias_buffer->first->effAddr == effAddr)
          {
            DPRINTF(AliasCache, " AccessStoreQueue::Found an alias in Alias Cache Store Queue:: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                  exe_alias_buffer->first->seqNum, 
                  exe_alias_buffer->first->effAddr,
                  exe_alias_buffer->second
                  );
        
            *pid = exe_alias_buffer->second;
            inst->setAliasStoreSeqNum(exe_alias_buffer->first->seqNum);
            return true;  // found in SQ
          }

       }

        DPRINTF(AliasCache, " AccessStoreQueue::Cannot Find an alias in Alias Cache Store Queue for EffAddr: 0x%x\n", 
                effAddr);
       return false; // not in SQ
    }

    template <class Impl>
    bool LRUAliasCache<Impl>::SquashEntry(uint64_t squashed_num)
    {

        auto exe_alias_table = ExeAliasTableBuffer.begin();

        while(exe_alias_table != ExeAliasTableBuffer.end()) {

            if(exe_alias_table->first->seqNum == squashed_num) {

                DPRINTF(AliasCache, "Alias Cache SquashEntry:: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                      exe_alias_table->first->seqNum, 
                      exe_alias_table->first->effAddr,
                      exe_alias_table->second
                      );
                exe_alias_table = ExeAliasTableBuffer.erase(exe_alias_table);
                return true;
            }
            else 
            {
              ++exe_alias_table;
            }
        }

      return false;
    }

    template <class Impl>
    void LRUAliasCache<Impl>::print_stats() {
        printf("Alias Cache Stats: %lu, %lu, %lu, %f \n",
        total_accesses, total_hits, total_misses,
        (double)total_hits/total_accesses
        );
    }
    template <class Impl>
    void LRUAliasCache<Impl>::DumpAliasTableBuffer (){
      //dump for debugging
      for (auto& entry : ExeAliasTableBuffer) {
        DPRINTF(AliasCache, "Alias Cache:: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                  entry.first->seqNum, 
                  entry.first->effAddr,
                  entry.second
                  );
      }

    }
    template <class Impl>
    void LRUAliasCache<Impl>::DumpShadowMemory (ThreadContext* tc){
      //dump for debugging
      for (auto const it_lv1 : tc->ShadowMemory)
      {
        for (auto const it_lv2 : it_lv1.second)
        {
            DPRINTF(AliasCache, "ShadowMemory[0x%x][0x%x]=%s\n", 
                    it_lv1.first, it_lv2.first, it_lv2.second);
        }

      }

    }
    template <class Impl>
    void LRUAliasCache<Impl>::DumpAliasCache (){
      //dump for debugging
      for (size_t setNum = 0; setNum < NumSets; setNum++) {
          for (size_t wayNum = 0; wayNum < NumWays; wayNum++) {
            if (AliasCache[setNum][wayNum].valid)
            {
                DPRINTF(AliasCache, "Alias Cache[%d][%d]=(0x%x,%s,%d)\n", 
                    setNum, wayNum,
                    AliasCache[setNum][wayNum].vaddr, 
                    AliasCache[setNum][wayNum].pid,
                    AliasCache[setNum][wayNum].dirty
                  );
            }
          }
      }


    }

    template <class Impl>
    void LRUAliasCache<Impl>::WriteBack(Addr wb_addr){
        //first search in the qeue, if the address is there,
        // update it else just put it into the front
        if (std::find(WbBuffer.begin(), WbBuffer.end(), wb_addr) !=
                      WbBuffer.end())
        {
          return;
        }
        else {
          WbBuffer.push_front(wb_addr);
        }

        // if the number of writes is greater than threshold,
        // write back the store
        if (WbBuffer.size() > 24) // wb max threshold = 24
        {
          while (WbBuffer.size() > 8) // wb low threshold = 8
          {
            WbBuffer.pop_back();
            outstandingWrite++;
          }
        }
    }
    template <class Impl>
    bool LRUAliasCache<Impl>::UpdateEntry(DynInstPtr& inst, ThreadContext* tc)
    {

        Addr effAddr = inst->effAddr;
        uint64_t storeSeqNum = inst->seqNum;

        DPRINTF(AliasCache, "UpdateEntry: Updating Alias Cache InsertStoreQueue Entry SeqNum: %d EffAddr: 0x%x\n", 
                storeSeqNum, effAddr);
        
        for (auto exe_alias_table = ExeAliasTableBuffer.begin();
                  exe_alias_table != ExeAliasTableBuffer.end();
                  exe_alias_table++)
        {
          if (exe_alias_table->first->seqNum == storeSeqNum)
          {
              DPRINTF(AliasCache, "Alias Cache Update Entry:: SeqNum: %d EffAddr: 0x%x PID=%s\n", 
                    exe_alias_table->first->seqNum, 
                    exe_alias_table->first->effAddr,
                    exe_alias_table->second
                    );
              assert(exe_alias_table->first->effAddr == effAddr && 
                    "Store Seq Number and Effective Address do not match!\n");
              DumpAliasTableBuffer();
              return true;
          }
        }

        return false;

    }


  
