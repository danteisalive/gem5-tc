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

#ifndef __ARCH_X86_TYPENODE_HH__
#define __ARCH_X86_TYPENODE_HH__

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <vector>
#include <bits/stdc++.h>
#include <unordered_map>
#include <queue>
//#include <boost/algorithm/string.hpp>

#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "cpu/simple/WordFM.hh"
// #include "cpu/static_inst.hh"
//#include "cpu/thread_context.hh"
#include "mem/page_table.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "debug/TypeMetadata.hh"
#include "debug/StackTypeMetadata.hh"
#include "base/loader/object_file.hh"
#include "base/loader/elf_object.hh"


namespace X86ISA
{
#define EFFECTIVE_PACKED            __attribute__((__packed__))
#define EFFECTIVE_VECTOR_SIZE(N)    __attribute__((__vector_size__(N)))
#define EFFECTIVE_RADIX             63
/*
 * Pre-defined hash values.
 */
#define EFFECTIVE_TYPE_NIL_HASH         ((uint64_t)-1)
#define EFFECTIVE_TYPE_INT8_HASH        0x703EDF97BC60677Dull   // Random
#define EFFECTIVE_TYPE_INT8_PTR_HASH    0x8D0DECDF6C6A8711ull   // Random

#define EFFECTIVE_COERCED_INT32_HASH    0x51A0B9BF4F692902ull   // Random
#define EFFECTIVE_COERCED_INT8_PTR_HASH 0x2317E969C295951Dull   // Random

class AllocationPointMeta
{
    private:
        bool Valid;
        std::string FileName;
        uint64_t     lineNum;
        uint64_t     ColNume;
        uint64_t    ConstValue;
        uint64_t    Hash1;
        uint64_t    Hash2;
        uint64_t    InlinedlineNum;
        uint64_t    InlinedColNume;
        uint64_t    IRBBID;
        uint64_t    IRInstID;
        uint64_t    MCBBID;
        uint64_t    MCInstID;
        std::string AllocatorName;
        std::string TypeName;
        std::string CallerName;
        uint64_t    TID;

    public:
        bool GetValidFlag() {return Valid;}
        void SetValidFlag(bool _valid) {Valid = _valid;}
        uint64_t GetConstValue() const {return ConstValue;}
        uint64_t GetHash1() const {return Hash1;}
        uint64_t GetHash2() const {return Hash2;}
        std::string GetAllocatorName() const {return AllocatorName;}

    public:
        AllocationPointMeta() {
            Valid = false;
            FileName = "";
            lineNum = 0;
            ColNume = 0;
            TypeName = "";
            ConstValue = 0;
            Hash1 = 0;
            Hash2 = 0;
            InlinedlineNum = 0;
            InlinedColNume = 0;
            IRBBID = 0;
            IRInstID = 0;
            MCBBID = 0;
            MCInstID = 0;
            AllocatorName = "";
            TypeName = "";
            CallerName = "";
        }

        AllocationPointMeta(std::string _CallerName) {
            Valid = true;
            FileName = "";
            lineNum = 0;
            ColNume = 0;
            TypeName = "";
            ConstValue = 0;
            Hash1 = 0;
            Hash2 = 0;
            InlinedlineNum = 0;
            InlinedColNume = 0;
            IRBBID = 0;
            IRInstID = 0;
            MCBBID = 0;
            MCInstID = 0;
            AllocatorName = "";
            TypeName = "";
            CallerName = _CallerName;
        }

        AllocationPointMeta(
                std::string _FileName,
                uint64_t     _lineNum,
                uint64_t     _ColNume,
                uint64_t    _ConstValue,
                uint64_t    _Hash1,
                uint64_t    _Hash2,
                uint64_t    _InlinedLineNum,
                uint64_t    _InlinedColNume,
                uint64_t    _IRBBID,
                uint64_t    _IRInstID,
                uint64_t    _TID,
                uint64_t    _MCBBID,
                uint64_t    _MCInstID,
                std::string _AllocatorName,
                std::string _TypeName,
                std::string _CallerName
                )
            {
                FileName =      _FileName;
                lineNum =       _lineNum;
                ColNume =       _ColNume;
                TypeName =      _TypeName;
                ConstValue =    _ConstValue;
                Hash1 =         _Hash1;
                Hash2 =         _Hash2;
                AllocatorName = _AllocatorName;
                Valid =         true;
                CallerName =    _CallerName;
                IRInstID =      _IRInstID;
                IRBBID =        _IRBBID;
                MCInstID =      _MCInstID;
                MCBBID  =       _MCBBID;
                TID =           _TID;
                InlinedlineNum = _InlinedLineNum;
                InlinedColNume = _InlinedColNume;

            }
        
        friend std::ostream& operator << (std::ostream& out, const AllocationPointMeta& apm )
        {
            assert(apm.Valid && "Printing an invalid AllocationPointMeta\n");

            ccprintf(out, "AllocationPointMeta:" 
                        "\n\tFileName= %s"
                        "\n\tlineNum = %d"
                        "\n\tColNume = %d"
                        "\n\tTypeName = %s"
                        "\n\tConstValue = %x" 
                        "\n\tHash1 = %x"
                        "\n\tHash2 = %x"
                        "\n\tAllocatorName = %s"
                        "\n\tCallerName = %s"
                        "\n\tIRInstID = %x"
                        "\n\tIRBBID = %x"
                        "\n\tMCInstID = %x"
                        "\n\tMCBBID = %x"
                        "\n\tTID = %x"
                        "\n\tInlinedColNume = %x"
                        "\n\tInlinedlineNum = %x\n",
                        apm.FileName,
                        apm.lineNum,
                        apm.ColNume,
                        apm.TypeName,
                        apm.ConstValue,
                        apm.Hash1,
                        apm.Hash2,
                        apm.AllocatorName,
                        apm.CallerName,
                        apm.IRInstID,
                        apm.IRBBID,
                        apm.MCInstID,
                        apm.MCBBID,
                        apm.TID,
                        apm.InlinedColNume,
                        apm.InlinedlineNum
                        );


            return out;
        }

        AllocationPointMeta (const AllocationPointMeta& apm)
        {
            this->FileName = apm.FileName;
            this->lineNum = apm.lineNum;
            this->ColNume = apm.ColNume;
            this->TypeName = apm.TypeName;
            this->ConstValue = apm.ConstValue;
            this->Hash1 = apm.Hash1;
            this->Hash2 = apm.Hash2;
            this->AllocatorName = apm.AllocatorName;
            this->Valid = apm.Valid;
            this->CallerName =   apm.CallerName;
            this->IRInstID =     apm.IRInstID;
            this->IRBBID =       apm.IRBBID;
            this->MCInstID =     apm.MCInstID;
            this->MCBBID  =      apm.MCBBID;
            this->TID =          apm.TID;
            this->InlinedColNume = apm.InlinedColNume;
            this->InlinedlineNum = apm.InlinedlineNum;
        }

        // A better implementation of operator =
        AllocationPointMeta& operator = (const AllocationPointMeta& apm)
        {
                // self-assignment guard
                if (this == &apm)
                    return *this;

                // do the copy
                this->FileName = apm.FileName;
                this->lineNum = apm.lineNum;
                this->ColNume = apm.ColNume;
                this->TypeName = apm.TypeName;
                this->ConstValue = apm.ConstValue;
                this->Hash1 = apm.Hash1;
                this->Hash2 = apm.Hash2;
                this->AllocatorName = apm.AllocatorName;
                this->Valid = apm.Valid;
                this->CallerName =      apm.CallerName;
                this->IRInstID =        apm.IRInstID;
                this->IRBBID =          apm.IRBBID;
                this->MCInstID =        apm.MCInstID;
                this->MCBBID  =         apm.MCBBID;
                this->TID =             apm.TID;
                this->InlinedColNume =  apm.InlinedColNume;
                this->InlinedlineNum =  apm.InlinedlineNum;
                // return the existing object so we can chain this operator
                return *this;
        }
};




class TyCHEAllocationPoint : public AllocationPointMeta {
        public:
            enum CheckType {
                AP_INVALID              = 0x0,
                AP_IGONRE               = 0x1,
                AP_BOUNDS_INJECT        = 0xd,
                AP_MALLOC_BASE_COLLECT  = 0xb,
                AP_MALLOC_SIZE_COLLECT  = 0xc,
                AP_FREE_CALL            = 0xe,
                AP_FREE_RET             = 0xf,
                AP_CALLOC_BASE_COLLECT  = 0x10,
                AP_CALLOC_SIZE_COLLECT  = 0x11,
                AP_REALLOC_BASE_COLLECT = 0x12,
                AP_REALLOC_SIZE_COLLECT  = 0x13,
                AP_STACK                = 0x14
            };

        private:
            CheckType ap_type;
            //AllocationPointMeta    ap_typeid;

   
        public:
        
            TyCHEAllocationPoint()
            {
                ap_type = CheckType::AP_INVALID;
            }
            TyCHEAllocationPoint(CheckType _ap_type, AllocationPointMeta _ap_typeid)
            :  AllocationPointMeta(_ap_typeid), ap_type(_ap_type)
            {

            }

            // copy constructor
            TyCHEAllocationPoint(const TyCHEAllocationPoint& empl)
               : AllocationPointMeta(empl), ap_type(empl.ap_type)
            {

            }

            //copy assignment
            TyCHEAllocationPoint& operator = (const TyCHEAllocationPoint & impl)
            {
                if (&impl == this)
                    return *this;
                
                (AllocationPointMeta&)(*this) = impl; 
                this->ap_type = impl.ap_type;

                return *this;
            }


            CheckType               GetCheckType() const {return ap_type;}
            static std::string      CheckTypeToStr(CheckType type)
            {
                switch (type) {
                    case CheckType::AP_IGONRE:
                        return "AP_IGONRE";
                    case CheckType::AP_INVALID:
                        return "AP_INVALID";
                    case CheckType::AP_MALLOC_BASE_COLLECT:
                        return "AP_MALLOC_BASE_COLLECT";
                    case CheckType::AP_MALLOC_SIZE_COLLECT:
                        return "AP_MALLOC_SIZE_COLLECT";
                    case CheckType::AP_BOUNDS_INJECT:
                        return "AP_BOUNDS_INJECT";
                    case CheckType::AP_FREE_CALL:
                        return "AP_FREE_CALL";
                    case CheckType::AP_FREE_RET:
                        return "AP_FREE_RET";
                    case CheckType::AP_CALLOC_BASE_COLLECT:
                        return "AP_CALLOC_BASE_COLLECT";
                    case CheckType::AP_CALLOC_SIZE_COLLECT:
                        return "AP_CALLOC_SIZE_COLLECT";
                    case CheckType::AP_REALLOC_BASE_COLLECT:
                        return "AP_REALLOC_BASE_COLLECT";
                    case CheckType::AP_REALLOC_SIZE_COLLECT:
                        return "AP_REALLOC_SIZE_COLLECT";
                    case CheckType::AP_STACK:
                        return "AP_STACK";
                    default:
                    {
                        assert(0);
                        return "Unrecognized Check!";
                    }
                }
        }
                    
};

class TypeEntryInfo
{
    private:
        bool        Valid;
        uint64_t    Offset;
        bool        Coerced;
        uint64_t    LowerBound;
        uint64_t    UpperBound;
        bool        FlexibleMemberArray;
        std::string Name;
        bool        VirtualTablePointer;
        std::string MetaType;
        std::string ParentType;

    public:
        TypeEntryInfo(
            uint64_t    off,
            bool        c, 
            uint64_t    lb,
            uint64_t    ub,
            bool        fma,
            std::string name,
            bool        vtpr,
            std::string meta_type,
            std::string parent_type
        )
        {
            Valid = true;
            Offset = off;
            Coerced = c;
            LowerBound = lb;
            UpperBound = ub;
            FlexibleMemberArray = fma;
            Name = name;
            VirtualTablePointer = vtpr;
            MetaType = meta_type;
            ParentType = parent_type;
        }

        TypeEntryInfo()
        {
            Valid = false;
            Offset = 0;
            Coerced = false;
            LowerBound = 0;
            UpperBound = 0;
            FlexibleMemberArray = false;
            Name = "";
            VirtualTablePointer = false;
            MetaType = "";
            ParentType = "";
        }

        

        TypeEntryInfo(const TypeEntryInfo& tei)
        {
            this->Valid = tei.Valid;
            this->Offset = tei.Offset;
            this->Coerced = tei.Coerced;
            this->LowerBound = tei.LowerBound;
            this->UpperBound = tei.UpperBound;
            this->FlexibleMemberArray = tei.FlexibleMemberArray;
            this->Name = tei.Name;
            this->VirtualTablePointer = tei.VirtualTablePointer;
            this->MetaType = tei.MetaType;
            this->ParentType = tei.ParentType;
        }

        TypeEntryInfo& operator= (const TypeEntryInfo& tei)
        {
            if (this == &tei)
                return (*this);
            
            this->Valid = tei.Valid;
            this->Offset = tei.Offset;
            this->Coerced = tei.Coerced;
            this->LowerBound = tei.LowerBound;
            this->UpperBound = tei.UpperBound;
            this->FlexibleMemberArray = tei.FlexibleMemberArray;
            this->Name = tei.Name;
            this->VirtualTablePointer = tei.VirtualTablePointer;
            this->MetaType = tei.MetaType;
            this->ParentType = tei.ParentType;

            return (*this);
        }



        friend std::ostream& operator << (std::ostream& out, const TypeEntryInfo& tei )
        {
            assert(tei.Valid && "Printing an invalid TypeEntryInfo\n");

                    ccprintf(out, "TypeEntryInfo:" 
                                "\n\tOffset = %d"
                                "\n\tCoerced = %d"
                                "\n\tLower Bound = %x"
                                "\n\tUpper Bound = %x"
                                "\n\tFlexible Member Array = %d"
                                "\n\tName = %s"
                                "\n\tVirtual Table Pointer = %d"
                                "\n\tMeta Type = %s"
                                "\n\tParent Type = %s\n", 
                                tei.Offset,
                                tei.Coerced,
                                tei.LowerBound,
                                tei.UpperBound,
                                tei.FlexibleMemberArray,
                                tei.Name,
                                tei.VirtualTablePointer,
                                tei.MetaType,
                                tei.ParentType
                                );
                    return out;
        }


        uint64_t GetOffset() {return Offset;}
};


class TypeMetadataInfo 
{
    private:
        bool Valid;
        bool IsAllocationPointMetadata;
        std::string FileName;
        int AllocationPointSize;
        AllocationPointMeta AllocPointMeta;
        std::multimap<uint64_t, TypeEntryInfo> TypeEntrys;

    public:
        TypeMetadataInfo()
        {
            Valid = false;
            IsAllocationPointMetadata = false;
            FileName = "";
            AllocationPointSize = 0;
            TypeEntrys.clear();
            // AllocPointMeta = AllocationPointMeta;

        }

        TypeMetadataInfo(
            bool _Valid,
            bool _IsAllocationPointMetadata,
            std::string _FileName,
            int _AllocationPointSize,
            AllocationPointMeta _AllocPointMeta,
            std::multimap<uint64_t, TypeEntryInfo> _TypeEntrys
        )
        {
            Valid = _Valid;
            IsAllocationPointMetadata = _IsAllocationPointMetadata;
            FileName = _FileName;
            AllocationPointSize = _AllocationPointSize;
            TypeEntrys = _TypeEntrys;
            AllocPointMeta = _AllocPointMeta;

        }


        TypeMetadataInfo(const TypeMetadataInfo& tmi)
        {
            this->Valid = tmi.Valid;
            this->IsAllocationPointMetadata = tmi.IsAllocationPointMetadata;
            this->FileName = tmi.FileName;
            this->AllocationPointSize = tmi.AllocationPointSize;
            this->TypeEntrys = tmi.TypeEntrys;
            this->AllocPointMeta = tmi.AllocPointMeta; 
        }

        TypeMetadataInfo& operator = (const TypeMetadataInfo& tmi)
        {
            if (this == &tmi)
                return (*this);

            this->Valid = tmi.Valid;
            this->IsAllocationPointMetadata = tmi.IsAllocationPointMetadata;
            this->FileName = tmi.FileName;
            this->AllocationPointSize = tmi.AllocationPointSize;
            this->TypeEntrys = tmi.TypeEntrys;
            this->AllocPointMeta = tmi.AllocPointMeta; 

            return (*this);
            
        }
        
        void SetValidFlag (bool flag) {Valid = flag;}
        void SetAllocationPointSize(int _AllocationPointSize) {AllocationPointSize = _AllocationPointSize;}
        bool GetValidFlag () {return Valid;}
        void ResetTypeEntrys() {TypeEntrys.clear();}
        void SetFileName(std::string filename) {FileName = filename;}
        void SetIsAllocationPointMetadata(bool _IsAllocationPointMetadata) {IsAllocationPointMetadata = _IsAllocationPointMetadata;}
        bool GetIsAllocationPointMetadata() const {return IsAllocationPointMetadata;}
        void SetAllocationPointMeta(AllocationPointMeta& _AllocPointMeta) {AllocPointMeta = _AllocPointMeta;}
        AllocationPointMeta GetAllocationPointMeta() const {return AllocPointMeta;}
        void InsertTypeEntry(uint64_t offset, TypeEntryInfo& tei)
        {
            TypeEntrys.insert(std::make_pair(offset, tei));
        }



        friend std::ostream& operator << (std::ostream& out, const TypeMetadataInfo& tmi)
        {
            assert(tmi.Valid && "Printing an invalid TypeMetadataInfo\n");
            
            if (tmi.IsAllocationPointMetadata)
                ccprintf(out, "TypeMetadataInfo:" 
                                "\n\tIsAllocationPointMetadata = %d"
                                "\n\tFileName = %s"
                                "\n\tAllocPointMeta = %s"
                                "\n\tAllocationPointSize = %d" 
                                "\n\tTypeEntrys = \n",
                                tmi.IsAllocationPointMetadata,
                                tmi.FileName,
                                tmi.AllocPointMeta,
                                tmi.AllocationPointSize
                                );
            else 
                ccprintf(out, "TypeMetadataInfo:" 
                                "\n\tIsAllocationPointMetadata = %d"
                                "\n\tFileName = %s"
                                "\n\tAllocationPointSize = %d" 
                                "\n\tTypeEntrys = \n",
                                tmi.IsAllocationPointMetadata,
                                tmi.FileName,
                                tmi.AllocationPointSize
                                );        
                    
            for (auto const & elem : tmi.TypeEntrys)
            {
                ccprintf(out, "\tOffset[%d]=\n\t%s\n", elem.first, elem.second);
            }


            return out;
        }
        
};



class PointerID
{
    private:
        uint64_t PID;
        uint64_t TID;

    public:

        PointerID()
        {
            PID = 0;
            TID = 0;
        }

        PointerID(uint64_t _PID)
        {
            PID = _PID;
            TID = 0;
        }
        
        PointerID(uint64_t _PID, uint64_t _TID)
        {
            PID = _PID;
            TID = _TID;
        }

        ~PointerID()
        {

        }

        bool operator != (const PointerID& _pid){
            return (PID != _pid.PID);
        }

        bool operator == (const PointerID& _pid){
            return (PID == _pid.PID);
        }


            // A better implementation of operator=
        PointerID& operator = (const PointerID& _pid)
        {
            // self-assignment guard
            if (this == &_pid)
                return *this;

            // do the copy
            this->PID   = _pid.PID;
            this->TID   = _pid.TID;
        
            // return the existing object so we can chain this operator
            return *this;
        }

        bool operator < (const PointerID& rhs) const {

            return (this->PID < rhs.PID);
        }

        PointerID(const PointerID& _pid)
        {
            this->PID  = _pid.PID;
            this->TID  = _pid.TID;
        }

        //void        SetTypeID(uint64_t _TypeID) { TypeID = _TypeID; }
        uint64_t    GetTypeID() const { return TID; }

        //void        SetPointerID(uint64_t _PointerID) { PointerID = _PointerID; }
        uint64_t    GetPointerID() const { return PID; }

};



    class AliasTableEntry
    {
      public:
        PointerID         pid;
        uint64_t          seqNum;
    };

    class CacheEntry {

        public:
            uint64_t tag;
            bool valid;
            bool dirty;
            uint64_t lruAge;
            uint64_t t_access_nums;
            uint64_t t_num_replaced;
            PointerID pid{0};
            uint64_t vaddr;
        public:
            CacheEntry():
                tag(0),valid(false), dirty(false),
                lruAge(1),t_access_nums(0), t_num_replaced(0), vaddr(0)
            {}

    } ;

    class LRUPIDCache
    {

    private:
        CacheEntry*   TheCache;
        int           MAX_SIZE;

        //TODO: move these to gem5 stats
      public:
        uint64_t                     total_accesses;
        uint64_t                     total_hits;
        uint64_t                     total_misses;

    public:
        LRUPIDCache(uint64_t _cache_size) :
            MAX_SIZE(_cache_size),
            total_accesses(0), total_hits(0), total_misses(0)
        {
          TheCache = new CacheEntry[MAX_SIZE];
        }

        ~LRUPIDCache(){
          delete [] TheCache;
        }

        bool LRUPIDCache_Access(uint64_t _pid_num) {
            //TODO: stats
            total_accesses = total_accesses + 1;

            Addr thisIsTheTag = _pid_num; //tag of the VA

            int j;
            int hit = 0;
            for (j = 0; j < MAX_SIZE; j++){

                if (TheCache[j].tag == thisIsTheTag){
                    //We have a hit!
                    hit = 1;
                    total_hits = total_hits + 1;
                    //Increase the age of everything
                    for (int k = 0; k < MAX_SIZE; k++){
                        TheCache[k].lruAge++;
                    }

                    TheCache[j].lruAge = 0;
                    TheCache[j].t_access_nums++;
                    break;

                }
            }

            if (hit == 0){

                total_misses = total_misses + 1;

                int m;
                int highestAge = 0;
                int highestSpot = 0;
                //Loop through the set and find the oldest element
                for (m = 0; m < MAX_SIZE; m++){
                    if (TheCache[m].lruAge > highestAge){
                        highestAge = TheCache[m].lruAge;
                        highestSpot = m;
                    }
                }

                //Replace the oldest element with the new tag
                TheCache[highestSpot].tag = thisIsTheTag;

                //Increase the age of each element
                for (m = 0; m < MAX_SIZE ; m++){
                    TheCache[m].lruAge++;
                }

                TheCache[highestSpot].lruAge = 0;
                TheCache[highestSpot].t_access_nums++;
                TheCache[highestSpot].t_num_replaced++;

            }

            if (hit == 1) return true;
            else return false;

      }

      void LRUPIDCachePrintStats() {
        printf("PID Cache Stats: %lu, %lu, %lu, %f \n",
        total_accesses, total_hits, total_misses,
        (double)total_hits/total_accesses
        );
      }

    };

    class LRUCache
    {

    private:
        CacheEntry*   TheCache;

        uint64_t                     NumWays;
        uint64_t                     NumSets;
        uint64_t                     CacheSize;
        uint64_t                     CacheBlockSize;
        uint64_t                     NumEntriesInCache;
        uint64_t                     BitsPerBlock;
        uint64_t                     BitsPerSet;
        uint64_t                     ShiftAmount;

        //TODO: move these to gem5 stats
        uint64_t                     total_accesses;
        uint64_t                     total_hits;
        uint64_t                     total_misses;

    public:
        LRUCache(uint64_t _num_ways,
                            uint64_t _cache_block_size,
                            uint64_t _cache_size) :
            NumWays(_num_ways), CacheSize(_cache_size),
            CacheBlockSize(_cache_block_size),
            total_accesses(0), total_hits(0), total_misses(0)
            {

                NumSets = (CacheSize / CacheBlockSize) / (NumWays);
                NumEntriesInCache = NumSets * NumWays;
                BitsPerSet = std::log2(NumSets);
                BitsPerBlock = std::log2(CacheBlockSize);
                ShiftAmount = BitsPerSet + BitsPerBlock;

                TheCache = new CacheEntry[NumEntriesInCache];

            }

        ~LRUCache(){
            delete [] TheCache;
        }

        void LRUCache_Access(Addr v_addr) {
            //TODO: stats
            total_accesses = total_accesses + 1;

            Addr thisIsTheTag = v_addr >> (ShiftAmount); //tag of the VA

            //Extract the set from the VA
            Addr thisIsTheSet =
                    ((v_addr -
                          ( thisIsTheTag <<
                            (BitsPerBlock + BitsPerSet))) >> BitsPerBlock
                    );
            int setLoopLow = thisIsTheSet * NumWays;
            int setLoopHigh = setLoopLow + (NumWays - 1);
            int j;
            int hit = 0;
            for(j = setLoopLow; j <= setLoopHigh; j++){

                if(TheCache[j].tag == thisIsTheTag){

                    //We have a hit!
                    hit = 1;
                    total_hits = total_hits + 1;
                    int k;
                    //Increase the age of everything
                    for( k = setLoopLow; k <= setLoopHigh; k++){
                        TheCache[k].lruAge++;
                    }

                    TheCache[j].lruAge = 0;
                    TheCache[j].t_access_nums++;
                    break;

                }
            }

            if(hit == 0){

                total_misses = total_misses + 1;

                int m;
                int highestAge = 0;
                int highestSpot = 0;
                //Loop through the set and find the oldest element
                for(m = setLoopLow; m <= setLoopHigh; m++){

                    if(TheCache[m].lruAge>highestAge){
                        highestAge = TheCache[m].lruAge;
                        highestSpot = m;
                    }

                }

                //Replace the oldest element with the new tag
                TheCache[highestSpot].tag = thisIsTheTag;

                //Increase the age of each element
                for(m=setLoopLow;m<=setLoopHigh;m++){
                    TheCache[m].lruAge++;
                }

                TheCache[highestSpot].lruAge = 0;
                TheCache[highestSpot].t_access_nums++;
                TheCache[highestSpot].t_num_replaced++;


            }

        }

      void LRUCachePrintStats() {
        printf("Capability Cache Stats: %lu, %lu, %lu, %f \n",
        total_accesses, total_hits, total_misses,
        (double)total_hits/total_accesses
        );
      }

    };




    inline static std::ostream &
            operator << (std::ostream & os, const PointerID & _pid)
    {
        ccprintf(os, "PID[%llu] TID[%x]",
                _pid.GetPointerID(), _pid.GetTypeID()
                );
        return os;
    }

    class Capability
    {
        private:
            int64_t begin;
            int64_t end;
            int64_t size;
            std::bitset<16> CSR;  //b0 = Executed, b1 = Commited

        public:
          uint64_t seqNum;

        public:
            Capability (){
                size = -1;
                begin = -1;
                end = -1;
                seqNum = 0;
                CSR.reset();
            }

            Capability(uint64_t _size)
            {
                begin = -1;
                end = -1;
                size = _size;
                seqNum = 0;
                CSR.reset();
            }
            ~Capability(){

            }

            // A better implementation of operator=
            Capability& operator = (const Capability &cap)
            {
                // self-assignment guard
                if (this == &cap)
                    return *this;

                // do the copy
                begin = cap.begin;
                end = cap.end;
                size = cap.size;
                CSR = cap.CSR;
                seqNum = cap.seqNum;
                // return the existing object so we can chain this operator
                return *this;
            }

            Capability(const Capability& _cap)
            {
                begin = _cap.begin;
                end = _cap.end;
                size = _cap.size;
                CSR = _cap.CSR;
                seqNum = _cap.seqNum;
            }


            void setBaseAddr(uint64_t _addr){
                                              if (size < 0) assert(0);
                                              begin = _addr;
                                              end = begin + size;
                                            }
            void            setSize (uint64_t _size){ size = _size;}
            uint64_t        getSize(){ return size; }
            uint64_t        getEndAddr(){ return begin + size;}
            uint64_t        getBaseAddr(){ return begin;}
            void            setCSRBit(int bit_num){ CSR.set(bit_num, 1);}
            void            clearCSRBit(int bit_num){ CSR.set(bit_num, 0);}
            bool            getCSRBit(int bit_num){ return CSR.test(bit_num);}
            std::bitset<16> getCSR(){return CSR;}
            void            reset(){ CSR.reset(); };

            bool contains(uint64_t _addr){
                // return false because cap is not commited yet
                if (!CSR.test(0)) return false;
                // check to make sure that we have both begin and end
                if (CSR.test(0)) { assert(begin > 0); assert(size > 0);}
                if (_addr >= begin && _addr <= end)  return true;
                else return false;
            }

    };


class StackSlot
{
    private:
        bool Valid;
        int Fi;
        unsigned long long Size;
        bool IsSpillSlot;
        uint64_t Alignment;
        bool Fixed;
        int Offset;
        AllocationPointMeta TypeInfo;
        bool hasMeta;
    
    public:
        StackSlot(
            int _Fi,
            unsigned long long _Size,
            bool _IsSpillSlot,
            uint64_t _Alignment,
            bool _Fixed,
            int _Offset,
            AllocationPointMeta _TypeInfo,
            bool _hasMeta
        )
        {
            Valid       = true;
            Fi          = _Fi          ;
            Size        = _Size        ;
            IsSpillSlot = _IsSpillSlot ;
            Alignment   = _Alignment   ;
            Fixed       = _Fixed       ;
            Offset      = _Offset      ;
            TypeInfo    = _TypeInfo    ;
            hasMeta     = _hasMeta      ;
        }

        StackSlot()
        {
            Valid = false;
        }

        

        StackSlot(const StackSlot& ss)
        {
            this->Valid       = ss.Valid     ;
            this->Fi          = ss.Fi          ;
            this->Size        = ss.Size        ;
            this->IsSpillSlot = ss.IsSpillSlot ;
            this->Alignment   = ss.Alignment   ;
            this->Fixed       = ss.Fixed       ;
            this->Offset      = ss.Offset      ;  
            this->TypeInfo    = ss.TypeInfo    ;   
            this->hasMeta     = ss.hasMeta      ;       
        }

        StackSlot& operator= (const StackSlot& ss)
        {
            if (this == &ss)
                return (*this);
            
            this->Valid       = ss.Valid     ;
            this->Fi          = ss.Fi          ;
            this->Size        = ss.Size        ;
            this->IsSpillSlot = ss.IsSpillSlot ;
            this->Alignment   = ss.Alignment   ;
            this->Fixed       = ss.Fixed       ;
            this->Offset      = ss.Offset      ; 
            this->TypeInfo    = ss.TypeInfo    ;   
            this->hasMeta     = ss.hasMeta      ;

            return (*this);
        }



        friend std::ostream& operator << (std::ostream& out, const StackSlot& ss )
        {
            assert(ss.Valid && "Printing an invalid StackSlot\n");
            if (ss.hasMeta)
                ccprintf(out, "StackSlot:"
                                " Fi = %d"
                                " Size = %llu"
                                " IsSpillSlot = %d"
                                " Alignment = %llu"
                                " Fixed = %d" 
                                " Offset = %d"
                                " TypeInfo = %s\n", 
                                ss.Fi,         
                                ss.Size,       
                                ss.IsSpillSlot,
                                ss.Alignment,
                                ss.Fixed,      
                                ss.Offset,
                                ss.TypeInfo       
                            );
            else 
                ccprintf(out, "StackSlot:"
                                " Fi = %d"
                                " Size = %llu"
                                " IsSpillSlot = %d"
                                " Alignment = %llu"
                                " Fixed = %d" 
                                " Offset = %d"
                                " TypeInfo = No Meta\n", 
                                ss.Fi,         
                                ss.Size,       
                                ss.IsSpillSlot,
                                ss.Alignment,
                                ss.Fixed,      
                                ss.Offset     
                            );

                    return out;
        }


};

class FunctionObject
{
    private:
        bool        Valid;
        std::string FunctionName;
        Addr        StartAddr;
        Addr        FunctionSize;
        //     OFFSET 
        std::map<int, StackSlot>            StackSlots;
        //      REG ID   TYPE INFO
        std::map<int, AllocationPointMeta>  ArgumentSlots;
        std::map<int, AllocationPointMeta>  ReturnTypes;

    public:
        FunctionObject(
            std::string _FunctionName,
            Addr _StartAddr,
            Addr _FunctionSize,
            std::map<int, StackSlot> _StackSlots,
            std::map<int, AllocationPointMeta> _ArgumentSlots,
            std::map<int, AllocationPointMeta> _ReturnType
        )
        {
            Valid = true;
            FunctionName = _FunctionName;
            StackSlots = _StackSlots;
            ArgumentSlots = _ArgumentSlots;
            ReturnTypes = _ReturnType;
            StartAddr = _StartAddr;
            FunctionSize = _FunctionSize;
        }

        FunctionObject()
        {
            Valid = false;
            FunctionName = "";
        }

        

        FunctionObject(const FunctionObject& so)
        {
            this->Valid = so.Valid;
            this->FunctionName = so.FunctionName;
            this->StackSlots = so.StackSlots;
            this->ReturnTypes = so.ReturnTypes;
            this->ArgumentSlots = so.ArgumentSlots;
            this->StartAddr = so.StartAddr;
            this->FunctionSize = so.FunctionSize;
        }

        FunctionObject& operator= (const FunctionObject& so)
        {
            if (this == &so)
                return (*this);
            
            this->Valid = so.Valid;
            this->FunctionName = so.FunctionName;
            this->StackSlots = so.StackSlots;
            this->ReturnTypes = so.ReturnTypes;
            this->ArgumentSlots = so.ArgumentSlots;
            this->StartAddr = so.StartAddr;
            this->FunctionSize = so.FunctionSize;
            return (*this);
        }



        friend std::ostream& operator << (std::ostream& out, const FunctionObject& so )
        {
            assert(so.Valid && "Printing an invalid FunctionObject\n");

                    ccprintf(out, "FunctionObject:" 
                                "\n\tFunction Name = %s StartAddr = %#x FunctionSize = %#x\n", 
                                so.FunctionName,
                                so.StartAddr,
                                so.FunctionSize
                                );
                    
                    for (auto& arg : so.ArgumentSlots)
                    {
                        ccprintf(out, "Argument[%d] = %s", 
                                    arg.first,
                                    arg.second
                                    );
                    }
                    
                    ccprintf(out, "\n");

                    for (auto& slot : so.StackSlots)
                    {
                        ccprintf(out, "Offset[%d] = %s", 
                                    slot.first,
                                    slot.second
                                    );
                    }
                    ccprintf(out, "\n");
                    
                    for (auto& ret : so.ReturnTypes)
                    {
                        ccprintf(out, "Return[%d] = %s", 
                                    ret.first,
                                    ret.second
                                    );
                    }
                    ccprintf(out, "\n");

                    return out;
        }

        std::pair<Addr,Addr> getFunctionStartAddressAndSize() const { return std::make_pair(StartAddr, FunctionSize); }

};



bool readVirtualTable(const char* file_name, ThreadContext *tc);  

bool readAllocationPointsSymbols(const char* file_name, ThreadContext *tc);
bool readTypeMetadata(const char* file_name, ThreadContext *tc);
bool readFunctionObjects(const char* exec_file_name, const char* stack_objects_file_name, ThreadContext *tc);


}




#endif // 
