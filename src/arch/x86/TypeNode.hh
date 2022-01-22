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

struct my_effective_info_entry {
    std::string global_name;
    uint32_t flags;
    size_t lb;
    size_t ub;
};
typedef struct my_effective_info_entry my_effective_info_entry;

struct my_effective_info {
    std::string global_name;
    uint64_t tid;
    uint64_t num_access;
    const std::string name;
    uint32_t size;
    uint32_t num_entries;
    uint32_t flags;
    std::string next;
    std::vector<my_effective_info_entry> entries;
};

typedef struct my_effective_info my_effective_info;


typedef struct EFFECTIVE_INFO EFFECTIVE_INFO;
typedef struct EFFECTIVE_TID EFFECTIVE_TID;

struct EFFECTIVE_TID
{
    uint64_t tid;
    uint64_t num_accesses;
};



typedef intptr_t EFFECTIVE_BOUNDS EFFECTIVE_VECTOR_SIZE(16);

struct EFFECTIVE_ENTRY
{
    uint64_t hash;              // Layout entry type.
    uint64_t _pad;              // Padding.
    EFFECTIVE_BOUNDS bounds;    // Sub-object bounds.
} EFFECTIVE_PACKED;

typedef struct EFFECTIVE_ENTRY EFFECTIVE_ENTRY;

#define EFFECTIVE_ENTRY_EMPTY_HASH  EFFECTIVE_TYPE_NIL_HASH


struct EFFECTIVE_INFO_ENTRY
{
    const EFFECTIVE_INFO *type;
    uint32_t flags;
    size_t lb;
    size_t ub;
};
typedef struct EFFECTIVE_INFO_ENTRY EFFECTIVE_INFO_ENTRY;

#define EFFECTIVE_INFO_ENTRY_FLAG_INHERITANCE   0x1
#define EFFECTIVE_INFO_ENTRY_FLAG_VIRTUAL       0x2

/*
 * Type info (for human readable type information).
 */
struct EFFECTIVE_INFO
{
    EFFECTIVE_TID *tid_info;
    const char *name;
    uint32_t size;
    uint32_t num_entries;
    uint32_t flags;
    const EFFECTIVE_INFO *next;
    EFFECTIVE_INFO_ENTRY entries[];
};

struct EFFECTIVE_TYPE
{   
    uint64_t hash;              // Type-specific hash value.
    uint64_t hash2;             // 2nd type-specific hash value.
    uint32_t size;              // sizeof(T)
    uint32_t size_fam;          // sizeof any FAM, else sizeof(T)
    uint32_t offset_fam;        // offset of 1st FAM element, else 0
    uint32_t sanity;            // EFFECTIVE_SANITY.
    size_t magic;               // Fixed-point (1 / size_fam) representation
    size_t mask;                // Mask for layout[]
    const EFFECTIVE_INFO *info; // Type info
    uint64_t next;              // Hash of next type coercion
    EFFECTIVE_ENTRY layout[];   // The layout hash table.
};

/*
 * Per-allocated-object meta-data representation.
 */
struct EFFECTIVE_META
{
    const EFFECTIVE_TYPE *type; // Object's effective type sans bounds.
    size_t size;                // Object's allocation size.
};

class TypeNode
{
public:
  
  TypeNode(std::string name, int tid) { this->name = name; this->typeID=tid;}
  int nodeNo;
  std::string title;
  std::string name;
  int typeID;
  std::vector<TypeNode *> childs;

  void deepCopy(TypeNode *source)
  {
    this->childs.clear();
    for (std::vector<TypeNode*>::iterator it = childs.begin(); it != childs.end() ;it++)
    {
      //std::cerr << "child's title is " << (*it)->title << '\n';
      TypeNode *newChild = new TypeNode((*it)->title, (*it)->typeID);
      this->childs.push_back(newChild);
      newChild->deepCopy((*it));
    }
  }

};

void printTypeTree(std::map<int, std::map<int, std::set<std::pair<int, int>>>>& typeTree, 
                  std::ofstream *out);

void buildTypeTree(my_effective_info ei,
                  std::map<std::string, my_effective_info> effInfos,
                  std::map<int, std::map<int, std::set<std::pair<int, int>>>>& typeTree);

void retreiveEffInfosFromFile(const std::string HashFileName, 
                            std::map<std::string, my_effective_info>& EffInfos);                

}

#endif // 
