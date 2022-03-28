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
#include "cpu/thread_context.hh"

namespace X86ISA {


bool readTypeMetadata(const char* file_name, ThreadContext *tc)
{

    std::ifstream input(file_name);
    
    std::string FILENAME = "";
    int APSIZE = -1; 
    uint64_t OFFSET = 0;
    bool CORECED = false; CORECED = CORECED;
    uint64_t LB = 0;
    uint64_t UB = 0;
    bool FMA = false; FMA = FMA;
    std::string NAME = "";
    bool VPTR = false;  VPTR = VPTR;
    std::string METATYPE = "";
    std::string PARENTTYPE = "";
    std::string METAID = "";

    TypeMetadataInfo type_metadata_info;
    type_metadata_info.SetFileName("");
    type_metadata_info.SetAllocationPointSize(0);
    type_metadata_info.ResetTypeEntrys();
    type_metadata_info.SetIsAllocationPointMetadata(false);
    type_metadata_info.SetValidFlag(false);

    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream iss(line);
        std::string key, answer;
        iss >> key;


        // DPRINTF(TypeMetadata, "KEY: %s\n", key);
        // DPRINTF(TypeMetadata, "RAW: %s\n", line);
        
        if (key == "FILENAME") 
        {
            answer = line.substr(9, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);

            type_metadata_info.SetFileName(answer);
            type_metadata_info.SetValidFlag(true);

        }
        else if (key == "APSIZE") 
        {
            answer = line.substr(7, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert(APSIZE == -1 && "APSIZE IS NOT -1 Before updating it!\n");
            APSIZE = std::stoi(answer);
            assert(APSIZE > 0 && "APSIZE IS NOT GREATER THAN 0!\n");
            assert(type_metadata_info.GetValidFlag() && "type_metadata_info IS NOT VALID!\n");
            type_metadata_info.SetAllocationPointSize(APSIZE);
            
        }
        else if (key == "OFFSET") 
        {
            answer = line.substr(7, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            OFFSET = std::stoull(answer);
            DPRINTF(TypeMetadata, "Key: %s Answer: %x\n", key, OFFSET);
            //assert(OFFSET >= 0 && "OFFSET IS NOT GREATER THAN 0!\n");
        }
        else if (key == "CORECED") 
        {
            answer = line.substr(8, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert((answer == "Y" || answer == "N") && "CORECED IS NOT Y or N!\n");
            CORECED = (answer == "Y") ? true : false;
        }
        else if (key == "LB") 
        {
            answer = line.substr(3, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            LB = std::stoull(answer);
            DPRINTF(TypeMetadata, "Key: %s Answer: %x\n", key, LB);
            //assert(LB >= 0 && "LB IS NOT GREATER THAN 0!\n");
        }
        else if (key == "UB") 
        {
            answer = line.substr(3, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            UB = std::stoull(answer);
            DPRINTF(TypeMetadata, "Key: %s Answer: %x\n", key, UB);
            //assert(UB >= 0 && "UB IS NOT GREATER THAN 0!\n");
        }
        else if (key == "FAM") 
        {
            answer = line.substr(4, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert((answer == "Y" || answer == "N") && "CORECED IS NOT Y or N!\n");
            FMA = (answer == "Y") ? true : false;
        } 
        else if (key == "NAME") 
        {
            answer = line.substr(5, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            NAME = answer;
        } 
        else if (key == "VPTR") 
        {
            answer = line.substr(5, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            VPTR = (answer == "Y") ? true : false;
        } 
        else if (key == "METATYPE") 
        {
            answer = line.substr(9, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            METATYPE = answer;
        } 
        else if (key == "PARENTTYPE") 
        {
            answer = line.substr(11, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            PARENTTYPE = answer;
            APSIZE--;
            assert(APSIZE >= 0 && "APSIZE is less than zero!\n");


            // here we have the whole Metadata Entry, insert it!
            
            TypeEntryInfo type_entry_info(OFFSET, 
                                        CORECED,
                                        LB,
                                        UB,
                                        FMA,
                                        NAME,
                                        VPTR,
                                        METATYPE,
                                        PARENTTYPE);

            type_metadata_info.InsertTypeEntry(type_entry_info.GetOffset(), type_entry_info);
            
        }
        else if (key == "METAID") 
        {
            // a type is only used when we have a meta id otherwise it's useless
            answer = line.substr(7, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            METAID = answer;

            

            // seperate the information by #
            std::string input = METAID;
            std::istringstream ss(input);
            std::string token;
            std::vector<std::string> tokens;
            while(std::getline(ss, token, '#')) {
                tokens.push_back(token);
            }

            assert(tokens.size() == 14 && "Tokens size is not equal to 14!\n" );

            
            // by the time we are here, APSIZE always should be zero! 
            // This is true only if this is a new type defenition
            assert(APSIZE == 0 && "METAID is seen and APSIZE is not zero!\n");

            FILENAME = "";
            APSIZE = -1;
            OFFSET = 0;
            CORECED = false;
            LB = 0;
            UB = 0;
            FMA = false;
            NAME = "";
            VPTR = false;
            METATYPE = "";
            PARENTTYPE = "";
            METAID = "";

            /*METAID 
                CWE843_Type_Confusion__char_82a.cpp#
                35#
                55#
                class CWE843_Type_Confusion__char_82::CWE843_Type_Confusion__char_82_bad#
                23505240#
                3368523464000242366#
                2501942056580049717#
                _Znwm#
                _ZN30CWE843_Type_Confusion__char_823badEv#
                35#
                55#
                23236960#
                23324768#
                2
            */   

            AllocationPointMeta _AllocPointMeta = AllocationPointMeta
                                                (
                                                    tokens[0], // FileName
                                                    ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                    ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                    ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // ConstValue
                                                    ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash1
                                                    ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // Hash2
                                                    ((tokens[9].size() != 0) ? std::stoull(tokens[9]) : 0), // inlined line 
                                                    ((tokens[10].size() != 0) ? std::stoull(tokens[10]) : 0), // inclined column 
                                                    ((tokens[11].size() != 0) ? std::stoull(tokens[11]) : 0), // BB ID
                                                    ((tokens[12].size() != 0) ? std::stoull(tokens[12]) : 0), // IR ID
                                                    ((tokens[13].size() != 0) ? std::stoull(tokens[13]) : 0), // TID
                                                    (0), // MCBBID
                                                    (0), // MCInstID
                                                    (tokens[7]), //Allocator Name
                                                    (tokens[3]), // TypeName
                                                    (tokens[8]) //Caller Name
                                                );
            


            assert(!type_metadata_info.GetAllocationPointMeta().GetValidFlag() && "AllocPointMeta is valid!\n");
            assert(!type_metadata_info.GetIsAllocationPointMetadata() && "IsAllocationPointMetadata is valid!\n");
            type_metadata_info.SetAllocationPointMeta(_AllocPointMeta);
            assert(type_metadata_info.GetAllocationPointMeta().GetValidFlag() && "AllocPointMeta is not valid!\n");

            type_metadata_info.SetIsAllocationPointMetadata(true);
            assert(type_metadata_info.GetValidFlag() && "type_metadata_info is not valid!\n");
            tc->TypeMetaDataBuffer.push_back(type_metadata_info); 


            AllocationPointMeta _AllocPointMetaNull;
            type_metadata_info.SetAllocationPointMeta(_AllocPointMetaNull);
            assert(!type_metadata_info.GetAllocationPointMeta().GetValidFlag() && "AllocPointMeta is valid!\n");
            type_metadata_info.SetFileName("");
            type_metadata_info.SetAllocationPointSize(0);
            type_metadata_info.ResetTypeEntrys();
            type_metadata_info.SetIsAllocationPointMetadata(false);
            type_metadata_info.SetValidFlag(false);
            
        } 
        else
        {
            DPRINTF(TypeMetadata, "Can't find any key: %s\n", key); 
            assert(0);
        }
        
        
    }

    return true;
}


bool readAllocationPointsSymbols(const char* file_name, ThreadContext *tc)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, ii, count;
    Elf64_Ehdr	*ehdr = NULL;
    std::map<int, Elf64_Word>  shs_flags;
    elf_version(EV_CURRENT);

    fd = open(file_name, O_RDONLY);
    panic_if(fd == -1, "readSymTab: Can't open file: %s! Error Number % d\n", std::string(file_name), errno);
    elf = elf_begin(fd, ELF_C_READ, NULL);
    //   std::cout << "readSymTab : " << std::string(file_name) << std::endl;
    assert (elf != NULL);

    int i = 0;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        shs_flags[i] = shdr.sh_flags;
        //printf("sh_number: %d sh_flags: %lu\n", i, shdr.sh_flags);
        i++;
    }

    
    ehdr = elf64_getehdr(elf);
    assert (elf != NULL);
    if (shs_flags.size() > ehdr->e_shnum){
        panic("invalid number of section headers!");
    }


    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            /* found a symbol table, go print it. */
            break;
        }
    }

    if (scn == NULL){
        panic("didn't found a symbol table!");
        return false;
    }


    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    std::map<Elf64_Addr, std::string> sym_name;
    std::map<Elf64_Addr, unsigned char> sym_info;
    std::map<Elf64_Addr, Elf64_Xword> sym_size;
    std::map<Elf64_Addr, Elf64_Half> sym_shndx;

    /* print the symbol names */
    for (ii = 0; ii < count; ++ii) 
    {
        GElf_Sym sym;
        gelf_getsym(data, ii, &sym);

        const char* pStr = elf_strptr(elf, shdr.sh_link, sym.st_name);
        std::string s1(pStr);



        size_t loc = 0;
        loc = s1.find("TYCHE_SYMS#", 0);
        if (loc == std::string::npos) continue;

        DPRINTF(TypeMetadata, "TYCHE Symbol Addr: %x Mangled Symbol: %s Symbol Size: %d Symbol Info: 0x%x Symbol Shndx: %d\n", 
                        sym.st_value, s1, sym.st_size, sym.st_info, sym.st_shndx);

        assert(loc == 0 && "Found a TYCHE symbol with strange name!\n"); 
        assert(sym.st_value != 0 && "Found a TYCHE symbol with value of 0!\n");
        assert(sym.st_size == 0 && "TYCHE sym size is not 0!\n");

        panic_if(sym_name.find(sym.st_value) != sym_name.end(), "duplicate sym_name! %x\n", sym.st_value);
        panic_if(sym_info.find(sym.st_value) != sym_info.end(), "duplicate sym_info!\n");
        panic_if(sym_size.find(sym.st_value) != sym_size.end(), "duplicate sym_size!\n");



        sym_name[sym.st_value] = s1;
        sym_info[sym.st_value] = sym.st_info;
        sym_size[sym.st_value] = sym.st_size;
        sym_shndx[sym.st_value] = sym.st_shndx;
    }

    
    elf_end(elf);
    close(fd);

    assert(sym_info.size());
    assert(sym_name.size());
    assert(sym_shndx.size());
    assert(sym_size.size());

    for (auto &sym : sym_name)
    {

        // seperate the information by #
        std::string input = sym.second.substr(11, sym.second.size() - 1);
        std::istringstream ss(input);
        std::string token;
        std::vector<std::string> tokens;
        while(std::getline(ss, token, '#')) {
            tokens.push_back(token);
        }

        if (tokens.size() != 16)
        {
            DPRINTF(TypeMetadata, "SYM: %s\n", input);
            for (size_t i = 0; i < tokens.size(); i++)
            {

                DPRINTF(TypeMetadata, "Tokens[%d] = %s\n", i, tokens[i]);
            }
            
            assert(tokens.size() == 16 && "Tokens size is not equal to 6!\n" );

        }


        DPRINTF(TypeMetadata, "Tokens[0]: %s "
                              "Tokens[1]: %s " 
                              "Tokens[2]: %s "
                              "Tokens[3]: %s "
                              "Tokens[4]: %s "
                              "Tokens[5]: %s "
                              "Tokens[6]: %s "
                              "Tokens[7]: %s "
                              "Tokens[8]: %s "
                              "Tokens[9]: %s "
                              "Tokens[10]: %s "
                              "Tokens[11]: %s "
                              "Tokens[12]: %s "
                              "Tokens[13]: %s "
                              "Tokens[14]: %s "
                              "Tokens[15]: %s\n",
                              tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],
                              tokens[6],tokens[7],tokens[8],tokens[9],tokens[10],tokens[11],
                              tokens[12],tokens[13],tokens[14],tokens[15]);

/*
TYCHE_SYMS#
    CWE843_Type_Confusion__char_82a.cpp#
    35#
    55#
    23505240#
    3368523464000242366#
    2501942056580049717#
    35#
    55#
    23236960#
    23324768#
    2#
    23324768#
    23236960#
    _Znwm#
    class CWE843_Type_Confusion__char_82::CWE843_Type_Confusion__char_82_bad#
    _ZN30CWE843_Type_Confusion__char_823badEv#
*/
     
        AllocationPointMeta _AllocPointMeta = AllocationPointMeta
                                                (
                                                    tokens[0], // FileName
                                                    ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                    ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                    ((tokens[3].size() != 0) ? std::stoull(tokens[3]) : 0), // ConstValue
                                                    ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // Hash1
                                                    ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash2
                                                    ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // inlined line 
                                                    ((tokens[7].size() != 0) ? std::stoull(tokens[7]) : 0), // inclined column 
                                                    ((tokens[8].size() != 0) ? std::stoull(tokens[8]) : 0), // BB ID
                                                    ((tokens[9].size() != 0) ? std::stoull(tokens[9]) : 0), // IR ID
                                                    ((tokens[10].size() != 0) ? std::stoull(tokens[10]) : 0), // TID
                                                    ((tokens[11].size() != 0) ? std::stoull(tokens[11]) : 0), // MC BB ID
                                                    ((tokens[12].size() != 0) ? std::stoull(tokens[12]) : 0), // MC Inst. ID
                                                    (tokens[13]), //Allocator Name                                                    
                                                    (tokens[14]), // TypeName
                                                    (tokens[15]) //Caller Name
                                                );

        tc->AllocationPointMetaBuffer.insert(std::make_pair(sym.first, _AllocPointMeta));
    }

    return true;
}

bool readVirtualTable(const char* file_name, ThreadContext *tc)
{

    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, ii, count;
    Elf64_Ehdr	*ehdr = NULL;
    std::map<int, Elf64_Word>  shs_flags;
    elf_version(EV_CURRENT);

    fd = open(file_name, O_RDONLY);
    panic_if(fd == -1, "readSymTab: Can't open file: %s! Error Number % d\n", std::string(file_name), errno);
    elf = elf_begin(fd, ELF_C_READ, NULL);
    //   std::cout << "readSymTab : " << std::string(file_name) << std::endl;
    assert (elf != NULL);

    int i = 0;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        shs_flags[i] = shdr.sh_flags;
        //printf("sh_number: %d sh_flags: %lu\n", i, shdr.sh_flags);
        i++;
    }

    
    ehdr = elf64_getehdr(elf);
    assert (elf != NULL);
    if (shs_flags.size() > ehdr->e_shnum){
        panic("invalid number of section headers!");
    }


    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            /* found a symbol table, go print it. */
            break;
        }
    }

    if (scn == NULL){
        panic("didn't found a symbol table!");
        return false;
    }


    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    std::map<Elf64_Addr, std::string> sym_name;
    std::map<Elf64_Addr, unsigned char> sym_info;
    std::map<Elf64_Addr, Elf64_Xword> sym_size;
    std::map<Elf64_Addr, Elf64_Half> sym_shndx;


    // now read the content of the virtual tables
    ObjectFile *lib = createObjectFile(file_name); 

    ElfObject *elf_obj = dynamic_cast<ElfObject*>(lib);

    /* print the symbol names */
    for (ii = 0; ii < count; ++ii) 
    {
        GElf_Sym sym;
        gelf_getsym(data, ii, &sym);

        // read all the symbols
        if (sym.st_value != 0)
        {

            const char* pStr = elf_strptr(elf, shdr.sh_link, sym.st_name);
            std::string s1(pStr);

            elf_obj->obj_sym_infos[sym.st_value]    = sym.st_info;
            elf_obj->obj_sym_names[sym.st_value]    = s1;
            elf_obj->obj_sym_shndxs[sym.st_value]   = sym.st_shndx;
            elf_obj->obj_sym_sizes[sym.st_value]    = sym.st_size;

            if (s1.find("_ZTV", 0) != 0) continue;

            panic_if(sym_name.find(sym.st_value) != sym_name.end(), "duplicate sym_name! %x\n", sym.st_value);
            panic_if(sym_info.find(sym.st_value) != sym_info.end(), "duplicate sym_info!\n");
            panic_if(!sym.st_size, "VPTR sym size is 0! %x\n", sym.st_value);
            panic_if(sym_size.find(sym.st_value) != sym_size.end(), "duplicate sym_size!\n");

            DPRINTF(TypeMetadata, "VTable Symbol Addr: %x Mangled Symbol: %s Symbol Size: %d Symbol Info: 0x%x Symbol Shndx: %d\n", 
                        sym.st_value, s1, sym.st_size, sym.st_info, sym.st_shndx);

            sym_name[sym.st_value] = s1;
            sym_info[sym.st_value] = sym.st_info;
            sym_size[sym.st_value] = sym.st_size;
            sym_shndx[sym.st_value] = sym.st_shndx;
        }

    }
    elf_end(elf);
    close(fd);


    DPRINTF(TypeMetadata, "\n\n");
    assert(elf_obj && "ElfObject is null!\n");
    // assert(sym_info.size());
    // assert(sym_name.size());
    // assert(sym_shndx.size());
    // assert(sym_size.size());


    // extract all the vtables
    for (auto &symbol : sym_name)
    {
        int status = 0;
    
        char *res = abi::__cxa_demangle(symbol.second.c_str(), NULL, NULL, &status);
        if (status != 0) {
            continue;
        }
        std::string demangled_name = std::string(res);
        if (demangled_name.find("vtable for ") != std::string::npos)
        {
            DPRINTF(TypeMetadata, "Symbol Addr: %x Symbol: %s Demangled Symbol: %s Symbol Size: %d Symbol Info: 0x%x Symbol Shndx: %d\n", 
                      symbol.first, symbol.second, demangled_name, sym_size[symbol.first], sym_info[symbol.first], sym_shndx[symbol.first]);
            elf_obj->readSectionData((int)sym_shndx[symbol.first], symbol.first, sym_size[symbol.first]);
            free(res);
        }
        else 
        {
            DPRINTF(TypeMetadata, "Symbol: %s\n", symbol.second);
            assert("A symbol that starts with _ZTV!\n");
        }
    }

    for (auto const &elem: elf_obj->getVirtualTables())
    {
        for (auto const &vtable : elem.second)
        {
            tc->VirtualTablesBuffer[elem.first].push_back(vtable);
        }
    }
    // tc->VirtualTablesBuffer = elf_obj->getVirtualTables();

    return false;
}

bool readFunctionObjects(const char* exec_file_name, const char* stack_objects_file_name, ThreadContext *tc)
{

    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, ii, count;
    Elf64_Ehdr	*ehdr = NULL;
    std::map<int, Elf64_Word>  shs_flags;
    elf_version(EV_CURRENT);

    fd = open(exec_file_name, O_RDONLY);
    panic_if(fd == -1, "readFunctionObjects: Can't open file: %s! Error Number % d\n", std::string(exec_file_name), errno);
    elf = elf_begin(fd, ELF_C_READ, NULL);
    //   std::cout << "readFunctionObjects : " << std::string(exec_file_name) << std::endl;
    assert (elf != NULL);

    int i = 0;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        shs_flags[i] = shdr.sh_flags;
        //printf("sh_number: %d sh_flags: %lu\n", i, shdr.sh_flags);
        i++;
    }

    
    ehdr = elf64_getehdr(elf);
    assert (elf != NULL);
    if (shs_flags.size() > ehdr->e_shnum){
        panic("invalid number of section headers!");
    }


    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            /* found a symbol table, go print it. */
            break;
        }
    }

    if (scn == NULL){
        panic("didn't found a symbol table!");
        return false;
    }


    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    std::map<Elf64_Addr, std::string> sym_names;
    std::map<Elf64_Addr, unsigned char> sym_infos;
    std::map<Elf64_Addr, Elf64_Xword> sym_sizes;
    std::map<Elf64_Addr, Elf64_Half> sym_shndxs;


    /* print the symbol names */
    for (ii = 0; ii < count; ++ii) 
    {
        GElf_Sym sym;
        gelf_getsym(data, ii, &sym);

        // read all the symbols
        if (sym.st_value != 0)
        {

            const char* pStr = elf_strptr(elf, shdr.sh_link, sym.st_name);
            std::string s1(pStr);

            sym_infos[sym.st_value]    = sym.st_info;
            sym_names[sym.st_value]    = s1;
            sym_shndxs[sym.st_value]   = sym.st_shndx;
            sym_sizes[sym.st_value]    = sym.st_size;

            DPRINTF(StackTypeMetadata, "VTable Symbol Addr: %x Mangled Symbol: %s Symbol Size: %d Symbol Info: 0x%x Symbol Shndx: %d\n", 
                        sym.st_value, s1, sym.st_size, sym.st_info, sym.st_shndx);

            sym_names[sym.st_value] = s1;
            sym_infos[sym.st_value] = sym.st_info;
            sym_sizes[sym.st_value] = sym.st_size;
            sym_shndxs[sym.st_value] = sym.st_shndx;
        }

    }
    elf_end(elf);
    close(fd);


    DPRINTF(StackTypeMetadata, "\n\n");

    std::ifstream input(stack_objects_file_name);
    
    std::string line;
    std::map<int, StackSlot>     stackSlots;
    std::map<int, AllocationPointMeta> argumetSlots;
    std::map<int, AllocationPointMeta> returnTypeSlots;
    while (std::getline(input, line))
    {
        std::istringstream iss(line);
        std::string key, answer;
        iss >> key;

        std::string functionName = "";
        int numberOfArguments = 0;
        int numberOfObjectsOnStack = 0;
        int numberOfReturnObject = 0;

        /*
        int fi
        unsigned long long size (0, ~ULL, > 0)
        bool isSpillSlot
        uint64_t Alignment
        bool fixed
        int offset
        */
        int fi = INT_MIN;
        unsigned long long size = UINT64_MAX;
        int isSpillSlot = -1;
        uint64_t alignment = UINT64_MAX;
        int fixed = -1;
        int offset = INT_MIN;
        // DPRINTF(StackTypeMetadata, "KEY: %s\n", key);
        // DPRINTF(StackTypeMetadata, "RAW: %s\n", line);
        
        if (key == "FN") 
        {
            answer = line.substr(3, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);
            functionName = answer;
        }
        else if (key == "NUM")
        {
            answer = line.substr(4, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);

            // seperate the information by space
            std::istringstream ss(answer);
            std::string token;
            std::vector<std::string> tokens;
            while(std::getline(ss, token, ' ')) {
                tokens.push_back(token);
            }

            if (tokens.size() != 14)
            {
                DPRINTF(StackTypeMetadata, "SYM: %s\n", answer);
                for (size_t i = 0; i < tokens.size(); i++)
                {
                    DPRINTF(StackTypeMetadata, "Tokens[%d] = %s\n", i, tokens[i]);
                }
                
                assert(tokens.size() == 3 && "Tokens size is not equal to 3!\n" );

            }        
            numberOfObjectsOnStack = std::stoi(tokens[0]);
            assert(numberOfObjectsOnStack >= 0 && "numberOfObjectsOnStack < 0\n");

            numberOfArguments = std::stoi(tokens[1]);
            assert(numberOfArguments >= 0 && "numberOfArguments < 0\n");

            numberOfReturnObject = std::stoi(tokens[2]);
            assert((numberOfReturnObject == 1) && 
                    "(numberOfReturnObject != 0 && numberOfReturnObject != 1)\n");
        }
        else if (key == "OBJ")
        {
            answer = line.substr(4, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert(numberOfObjectsOnStack >= 1 && "numberOfObjectsOnStack <= 0\n");


            std::stringstream ss(answer);
            ss >> fi >> size >> isSpillSlot >> alignment >> fixed >> offset;
            assert((isSpillSlot == 0 || isSpillSlot == 1) && "wrong value for isSpillSlot!\n");
            assert((fixed == 0 || fixed == 1) && "wrong value for fixed!\n");
            
        }
        else if (key == "OBJECTMETA")
        {
            answer = line.substr(11, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);

            if (answer == "NOMETA")
            {
                AllocationPointMeta _AllocPointMetaNull;
                stackSlots[offset] = StackSlot(fi, 
                                        size, 
                                        isSpillSlot == 1 ? true : false, 
                                        alignment, 
                                        fixed == 1 ? true : false, 
                                        offset,
                                        _AllocPointMetaNull
                                        );
                numberOfObjectsOnStack--;
            }
            else
            {

                // seperate the information by #
                std::istringstream ss(answer);
                std::string token;
                std::vector<std::string> tokens;
                while(std::getline(ss, token, '#')) {
                    tokens.push_back(token);
                }

                if (tokens.size() != 14)
                {
                    DPRINTF(StackTypeMetadata, "SYM: %s\n", answer);
                    for (size_t i = 0; i < tokens.size(); i++)
                    {

                        DPRINTF(StackTypeMetadata, "Tokens[%d] = %s\n", i, tokens[i]);
                    }
                    
                    assert(tokens.size() == 14 && "Tokens size is not equal to 14!\n" );

                }

                DPRINTF(StackTypeMetadata, "Tokens[0]: %s "
                                    "Tokens[1]: %s " 
                                    "Tokens[2]: %s "
                                    "Tokens[3]: %s "
                                    "Tokens[4]: %s "
                                    "Tokens[5]: %s "
                                    "Tokens[6]: %s "
                                    "Tokens[7]: %s "
                                    "Tokens[8]: %s "
                                    "Tokens[9]: %s "
                                    "Tokens[10]: %s "
                                    "Tokens[11]: %s "
                                    "Tokens[12]: %s "
                                    "Tokens[13]: %s\n",
                                    tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],
                                    tokens[6],tokens[7],tokens[8],tokens[9],tokens[10],tokens[11],
                                    tokens[12],tokens[13]);

                /*              
                CWE843_Type_Confusion__short_82_goodG2B.cpp# tokens[0] = filename
                18446744073709551615#   tokens[1] = line
                18446744073709551615#   tokens[2] = col 
                int8_t *#               tokens[3] = TypeName
                39020968#               tokens[4] = ConstValue
                526828848944628746#     tokens[5] = Hash1
                11854005139656696112#   tokens[6] = Hash2
                Alloca#                 tokens[7] = AllocatorName
                _ZN31CWE843_Type_Confusion__short_8239CWE843_Type_Confusion__short_82_goodG2B6actionEPv# tokens[8] = CallerName
                0#                       tokens[9] =        inlined line 
                0#                       tokens[10] =        inclined column 
                38927856#                tokens[11] =        BB ID
                38926376#                tokens[12] =        IR ID
                3#                       tokens[13] =        TID
                */
                AllocationPointMeta _AllocPointMeta = AllocationPointMeta
                                                        (
                                                            tokens[0], // FileName
                                                            ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                            ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                            ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // ConstValue
                                                            ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash1
                                                            ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // Hash2
                                                            ((tokens[9].size() != 0) ? std::stoull(tokens[9]) : 0), // inlined line 
                                                            ((tokens[10].size() != 0) ? std::stoull(tokens[10]) : 0), // inclined column 
                                                            ((tokens[11].size() != 0) ? std::stoull(tokens[11]) : 0), // BB ID
                                                            ((tokens[12].size() != 0) ? std::stoull(tokens[12]) : 0), // IR ID
                                                            ((tokens[13].size() != 0) ? std::stoull(tokens[13]) : 0), // TID
                                                            0, // MC BB ID
                                                            0, // MC Inst. ID
                                                            (tokens[7]), //Allocator Name                                                    
                                                            (tokens[3]), // TypeName
                                                            (tokens[8]) //Caller Name
                                                        );

                stackSlots[offset] = StackSlot(fi, 
                                        size, 
                                        isSpillSlot == 1 ? true : false, 
                                        alignment, 
                                        fixed == 1 ? true : false, 
                                        offset,
                                        _AllocPointMeta
                                        );
                
                numberOfObjectsOnStack--;
            }
   
        }
        else if (key == "ARGMETA")
        {
            answer = line.substr(8, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert(numberOfArguments > 0 && "numberOfArguments <= 0!\n");
            if (answer == "NOMETA")
            {
                AllocationPointMeta _AllocPointMetaNull;
                argumetSlots[numberOfArguments] = _AllocPointMetaNull;
                numberOfArguments--;
            }
            else
            {

                // seperate the information by #
                std::istringstream ss(answer);
                std::string token;
                std::vector<std::string> tokens;
                while(std::getline(ss, token, '#')) {
                    tokens.push_back(token);
                }

                if (tokens.size() != 14)
                {
                    DPRINTF(StackTypeMetadata, "SYM: %s\n", answer);
                    for (size_t i = 0; i < tokens.size(); i++)
                    {

                        DPRINTF(StackTypeMetadata, "Tokens[%d] = %s\n", i, tokens[i]);
                    }
                    
                    assert(tokens.size() == 14 && "Tokens size is not equal to 14!\n" );

                }

                DPRINTF(StackTypeMetadata, "Tokens[0]: %s "
                                    "Tokens[1]: %s " 
                                    "Tokens[2]: %s "
                                    "Tokens[3]: %s "
                                    "Tokens[4]: %s "
                                    "Tokens[5]: %s "
                                    "Tokens[6]: %s "
                                    "Tokens[7]: %s "
                                    "Tokens[8]: %s "
                                    "Tokens[9]: %s "
                                    "Tokens[10]: %s "
                                    "Tokens[11]: %s "
                                    "Tokens[12]: %s "
                                    "Tokens[13]: %s\n",
                                    tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],
                                    tokens[6],tokens[7],tokens[8],tokens[9],tokens[10],tokens[11],
                                    tokens[12],tokens[13]);

                /*              
                CWE843_Type_Confusion__short_82_goodG2B.cpp# tokens[0] = filename
                18446744073709551615#   tokens[1] = line
                18446744073709551615#   tokens[2] = col 
                int8_t *#               tokens[3] = TypeName
                39020968#               tokens[4] = ConstValue
                526828848944628746#     tokens[5] = Hash1
                11854005139656696112#   tokens[6] = Hash2
                Alloca#                 tokens[7] = AllocatorName
                _ZN31CWE843_Type_Confusion__short_8239CWE843_Type_Confusion__short_82_goodG2B6actionEPv# tokens[8] = CallerName
                0#                       tokens[9] =        inlined line 
                0#                       tokens[10] =        inclined column 
                38927856#                tokens[11] =        BB ID
                38926376#                tokens[12] =        IR ID
                3#                       tokens[13] =        TID
                */
                AllocationPointMeta _AllocPointMeta = AllocationPointMeta
                                                        (
                                                            tokens[0], // FileName
                                                            ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                            ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                            ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // ConstValue
                                                            ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash1
                                                            ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // Hash2
                                                            ((tokens[9].size() != 0) ? std::stoull(tokens[9]) : 0), // inlined line 
                                                            ((tokens[10].size() != 0) ? std::stoull(tokens[10]) : 0), // inclined column 
                                                            ((tokens[11].size() != 0) ? std::stoull(tokens[11]) : 0), // BB ID
                                                            ((tokens[12].size() != 0) ? std::stoull(tokens[12]) : 0), // IR ID
                                                            ((tokens[13].size() != 0) ? std::stoull(tokens[13]) : 0), // TID
                                                            0, // MC BB ID
                                                            0, // MC Inst. ID
                                                            (tokens[7]), //Allocator Name                                                    
                                                            (tokens[3]), // TypeName
                                                            (tokens[8]) //Caller Name
                                                        );

                argumetSlots[numberOfArguments] = _AllocPointMeta;
                
                numberOfArguments--;
            }
   
        }
        else if (key == "RETMETA")
        {
            answer = line.substr(8, line.size() - 1);
            DPRINTF(StackTypeMetadata, "Key: %s Answer: %s\n", key, answer);
            assert(numberOfReturnObject > 0 && "numberOfReturnObject <= 0!\n");
            if (answer == "NOMETA")
            {
                // do nothing
                AllocationPointMeta _returnTypeMeta;
                returnTypeSlots[numberOfReturnObject] = _returnTypeMeta;
                numberOfReturnObject--;
            }
            else
            {

                // seperate the information by #
                std::istringstream ss(answer);
                std::string token;
                std::vector<std::string> tokens;
                while(std::getline(ss, token, '#')) {
                    tokens.push_back(token);
                }

                if (tokens.size() != 14)
                {
                    DPRINTF(StackTypeMetadata, "SYM: %s\n", answer);
                    for (size_t i = 0; i < tokens.size(); i++)
                    {

                        DPRINTF(StackTypeMetadata, "Tokens[%d] = %s\n", i, tokens[i]);
                    }
                    
                    assert(tokens.size() == 14 && "Tokens size is not equal to 14!\n" );

                }

                DPRINTF(StackTypeMetadata, "Tokens[0]: %s "
                                    "Tokens[1]: %s " 
                                    "Tokens[2]: %s "
                                    "Tokens[3]: %s "
                                    "Tokens[4]: %s "
                                    "Tokens[5]: %s "
                                    "Tokens[6]: %s "
                                    "Tokens[7]: %s "
                                    "Tokens[8]: %s "
                                    "Tokens[9]: %s "
                                    "Tokens[10]: %s "
                                    "Tokens[11]: %s "
                                    "Tokens[12]: %s "
                                    "Tokens[13]: %s\n",
                                    tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],
                                    tokens[6],tokens[7],tokens[8],tokens[9],tokens[10],tokens[11],
                                    tokens[12],tokens[13]);

                /*              
                CWE843_Type_Confusion__short_82_goodG2B.cpp# tokens[0] = filename
                18446744073709551615#   tokens[1] = line
                18446744073709551615#   tokens[2] = col 
                int8_t *#               tokens[3] = TypeName
                39020968#               tokens[4] = ConstValue
                526828848944628746#     tokens[5] = Hash1
                11854005139656696112#   tokens[6] = Hash2
                Alloca#                 tokens[7] = AllocatorName
                _ZN31CWE843_Type_Confusion__short_8239CWE843_Type_Confusion__short_82_goodG2B6actionEPv# tokens[8] = CallerName
                0#                       tokens[9] =        inlined line 
                0#                       tokens[10] =        inclined column 
                38927856#                tokens[11] =        BB ID
                38926376#                tokens[12] =        IR ID
                3#                       tokens[13] =        TID
                */
                AllocationPointMeta _returnTypeMeta = AllocationPointMeta
                                                        (
                                                            tokens[0], // FileName
                                                            ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                            ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                            ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // ConstValue
                                                            ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash1
                                                            ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // Hash2
                                                            ((tokens[9].size() != 0) ? std::stoull(tokens[9]) : 0), // inlined line 
                                                            ((tokens[10].size() != 0) ? std::stoull(tokens[10]) : 0), // inclined column 
                                                            ((tokens[11].size() != 0) ? std::stoull(tokens[11]) : 0), // BB ID
                                                            ((tokens[12].size() != 0) ? std::stoull(tokens[12]) : 0), // IR ID
                                                            ((tokens[13].size() != 0) ? std::stoull(tokens[13]) : 0), // TID
                                                            0, // MC BB ID
                                                            0, // MC Inst. ID
                                                            (tokens[7]), //Allocator Name                                                    
                                                            (tokens[3]), // TypeName
                                                            (tokens[8]) //Caller Name
                                                        );
                returnTypeSlots[numberOfReturnObject] = _returnTypeMeta;
                numberOfReturnObject--;
            }

            if (numberOfReturnObject == 0)
            {
                    // now insert the whole function object into the TC
                    assert(functionName != "" && "Empty Function Name!\n");
                    // we should have parsed all the args and stack objects
                    assert(numberOfArguments == 0 && numberOfObjectsOnStack == 0 && "");
                    auto it = sym_names.begin();
                    for ( ; it != sym_names.end(); it++)
                    {
                        if (it->second == functionName)
                            break;
                    }
                    
                    assert(it != sym_names.end() && "");
                    tc->FunctionObjectsBuffer[it->first] = FunctionObject(functionName, stackSlots, argumetSlots, returnTypeSlots);
                    argumetSlots.clear();
                    stackSlots.clear();
                    returnTypeSlots.clear();
            }
            
            
        }
    }


    return true;

}


}




