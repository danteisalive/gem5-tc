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
#include <boost/algorithm/string.hpp>

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
#include "cpu/static_inst.hh"
#include "cpu/thread_context.hh"
#include "mem/page_table.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "debug/TypeMetadata.hh"
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
        int64_t     lineNum;
        int64_t     ColNume;
        std::string TypeName;
        uint64_t    ConstValue;
        uint64_t    Hash1;
        uint64_t    Hash2;
        std::string AllocatorName;

    public:
        bool GetValidFlag() {return Valid;}
        void SetValidFlag(bool _valid) {Valid = _valid;}
        uint64_t GetConstValue() const {return ConstValue;}
        uint64_t GetHash1() const {return Hash1;}
        uint64_t GetHash2() const {return Hash2;}

    public:
        AllocationPointMeta() {
            Valid = false;
            FileName = "";
            lineNum = -1;
            ColNume = -1;
            TypeName = "";
            ConstValue = 0;
            Hash1 = 0;
            Hash2 = 0;
            AllocatorName = "";
        }

        AllocationPointMeta(
                std::string _FileName,
                int64_t     _lineNum,
                int64_t     _ColNume,
                std::string _TypeName,
                uint64_t    _ConstValue,
                uint64_t    _Hash1,
                uint64_t    _Hash2,
                std::string _AllocatorName)
            {
                FileName = _FileName;
                lineNum = _lineNum;
                ColNume = _ColNume;
                TypeName = _TypeName;
                ConstValue = _ConstValue;
                Hash1 = _Hash1;
                Hash2 = _Hash2;
                AllocatorName = _AllocatorName;
                Valid = true;

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
                        "\n\tAllocatorName = %s\n",
                        apm.FileName,
                        apm.lineNum,
                        apm.ColNume,
                        apm.TypeName,
                        apm.ConstValue,
                        apm.Hash1,
                        apm.Hash2,
                        apm.AllocatorName
                        );


            return out;
        }

        AllocationPointMeta (const AllocationPointMeta& apm)
        {
            this->FileName = apm.FileName;
            this->FileName = apm.FileName;
            this->lineNum = apm.lineNum;
            this->ColNume = apm.ColNume;
            this->TypeName = apm.TypeName;
            this->ConstValue = apm.ConstValue;
            this->Hash1 = apm.Hash1;
            this->Hash2 = apm.Hash2;
            this->AllocatorName = apm.AllocatorName;
            this->Valid = apm.Valid;
        }

        // A better implementation of operator =
        AllocationPointMeta& operator = (const AllocationPointMeta& apm)
        {
                // self-assignment guard
                if (this == &apm)
                    return *this;

                // do the copy
                this->FileName = apm.FileName;
                this->FileName = apm.FileName;
                this->lineNum = apm.lineNum;
                this->ColNume = apm.ColNume;
                this->TypeName = apm.TypeName;
                this->ConstValue = apm.ConstValue;
                this->Hash1 = apm.Hash1;
                this->Hash2 = apm.Hash2;
                this->AllocatorName = apm.AllocatorName;
                this->Valid = apm.Valid;
                // return the existing object so we can chain this operator
                return *this;
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
   

bool readVirtualTable(const char* file_name, ThreadContext *tc);  

bool readAllocationPointsSymbols(const char* file_name, ThreadContext *tc);
bool readTypeMetadata(const char* file_name, ThreadContext *tc);


}




#endif // 
