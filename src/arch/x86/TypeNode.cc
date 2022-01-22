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



#include "arch/x86/TypeNode.hh"

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


namespace X86ISA {


void retreiveEffInfosFromFile(const std::string HashFileName, 
                            std::map<std::string, my_effective_info>& EffInfos) {
    
    std::ifstream afile;    
    afile.open(HashFileName.c_str(), std::ios::in);
    if(!afile)
    {
        assert(0);
    } 
    // std::ifstream afile(hashFileName.c_str());
    std::string line;
    std::vector<std::string> strs;

    while (std::getline(afile, line)) {
        std::cout << line << "\n" << std::flush;
        std::string eff_info_global_name;
        uint64_t tid;
        uint64_t access;
        std::string name;
        uint32_t size;
        uint32_t num_entries;
        uint32_t flags;
        std::string next;
        std::vector<my_effective_info_entry> entries;
        // std::cerr << line << '\n';
        boost::split(strs, line, boost::is_any_of("#"),
                     boost::token_compress_on);
        eff_info_global_name = strs[0];
        // To-change
        
        tid = atoi(strs[1].c_str());
        access = atol(strs[2].c_str());
        name = strs[3];
        size = atoi(strs[4].c_str());
        num_entries = atoi(strs[5].c_str());
        flags = atoi(strs[6].c_str());
        next = strs[7];

        //TIDNames[tid] = name;

        for (unsigned int i = 0; i < num_entries; i++) {
            int idx = 8 + i * 4;
            std::string global_name = strs[idx];
            std::cout  << global_name << std::endl << std::flush;
            uint32_t flags = atoi(strs[idx + 1].c_str());
            std::cout  << flags << std::endl << std::flush;
            size_t lb = (size_t)atoi(strs[idx + 2].c_str());
            std::cout  << lb << std::endl << std::flush;
            size_t ub = (size_t)atoi(strs[idx + 3].c_str());
            std::cout  << ub << std::endl << std::flush;
            std::cout  << EffInfos[global_name].size << std::endl << std::flush;
            assert(EffInfos.find(global_name) != EffInfos.end());
            if (EffInfos[global_name].size != (ub - lb)) {
                if (ub - lb > 0) {
                    assert(EffInfos[global_name].size != 0);
                    if ((ub - lb) % EffInfos[global_name].size == 0) {
                        uint32_t array_length =
                            (ub - lb) / EffInfos[global_name].size;
                        uint32_t entry_size = EffInfos[global_name].size;
                        for (uint32_t i = 0; i < array_length; i++) {
                            size_t new_lb = lb + i*entry_size;
                            size_t new_ub =  lb + (i+1)*entry_size;
                            my_effective_info_entry e_i_entry = {global_name,
                                                                 flags, new_lb, new_ub};
                            entries.push_back(e_i_entry);
                        }
                    } else {
                        //*out << "strage size of entry\n";
                    }
                }
            } else {
                my_effective_info_entry e_i_entry = {global_name, flags, lb,
                                                     ub};
                entries.push_back(e_i_entry);
            }
        }
        my_effective_info eff_info = {
            eff_info_global_name, tid,   access, name,   size,
            num_entries,          flags, next,   entries};
        EffInfos.insert(std::pair<std::string, my_effective_info>(
            eff_info_global_name, eff_info));
    }
    // *out << "Exited "
    //      << "\n";
    afile.close();
}

void buildTypeTree(my_effective_info ei,
                  std::map<std::string, my_effective_info> effInfos,
                  std::map<int, std::map<int, std::set<std::pair<int, int>>>>& typeTree) {

    typeTree[ei.tid][0].insert(std::pair<int, int>(ei.tid, ei.size));

    // std::map<int, std::map<int, std::set<std::pair<int, int> > > > typeTree;
    for (size_t i = 0; i < ei.entries.size(); i++) {
        my_effective_info_entry eie = ei.entries[i];
        uint64_t entryID = effInfos[eie.global_name].tid;
        size_t eie_lb = eie.lb;

        if (typeTree.find(entryID) == typeTree.end()) {
            my_effective_info ei_new = effInfos[eie.global_name];
            buildTypeTree(ei_new, effInfos, typeTree);
        }
        std::map<int, std::set<std::pair<int, int> > > entryMap =
            typeTree[entryID];
        std::map<int, std::set<std::pair<int, int> > >::iterator itr;
        for (itr = entryMap.begin(); itr != entryMap.end(); itr++) {
            size_t offset_new = itr->first;
            std::set<std::pair<int, int> > p_new = itr->second;
            std::set<std::pair<int, int> >::iterator setItr = p_new.begin();
            while (setItr != p_new.end()) {
                typeTree[ei.tid][eie_lb + offset_new].insert(*(setItr));
                setItr++;
            }
        }
    }
}
void printTypeTree(std::map<int, std::map<int, std::set<std::pair<int, int>>>>& typeTree, 
                  std::ofstream *out) {

    std::map<int, std::map<int, std::set<std::pair<int, int>>>>::iterator  itr;
    for (itr = typeTree.begin(); itr != typeTree.end(); itr++) {
        int tid = itr->first;
        *out << std::dec << "TypeID = " << tid << ": \n";
        std::map<int, std::set<std::pair<int, int> > > mp = itr->second;
        std::map<int, std::set<std::pair<int, int> > >::iterator mpItr =
            mp.begin();
        while (mpItr != mp.end()) {
            int offset = mpItr->first;
            *out << "Offset = " << offset;
            std::set<std::pair<int, int> > set = mpItr->second;
            std::set<std::pair<int, int> >::iterator setItr = set.begin();
            while (setItr != set.end()) {
                *out << ", Subobject TypeID = " << (*setItr).first
                     << ", Subobject Size = " << (*setItr).second;
                *out << "\n";
                setItr++;
            }

            mpItr++;
        }
    }
}


}

