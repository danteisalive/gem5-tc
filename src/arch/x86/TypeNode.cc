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


bool readTypeMetadata(const char* file_name, ThreadContext *tc)
{

    std::ifstream input(file_name);
    
    std::string FILENAME = "";
    int APSIZE = -1; 
    int OFFSET = 0;
    bool CORECED = false; CORECED = CORECED;
    int LB = 0;
    int UB = 0;
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


        DPRINTF(TypeMetadata, "KEY: %s\n", key);
        DPRINTF(TypeMetadata, "RAW: %s\n", line);
        
        if (key == "FILENAME") 
        {
            answer = line.substr(10, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            
            // if this is valid, it means this is not an Allocation Point metadata, save it for future!
            if(type_metadata_info.GetValidFlag())
            {
                assert(APSIZE == 0 && "APSIZE is not zero for a not allocation point metadata!\n");
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

                tc->TypeMetaDataBuffer.push_back(type_metadata_info);    
                type_metadata_info.SetFileName("");
                type_metadata_info.SetAllocationPointSize(0);
                type_metadata_info.ResetTypeEntrys();
                type_metadata_info.SetIsAllocationPointMetadata(false);
                type_metadata_info.SetValidFlag(false);
            }

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
            assert(OFFSET >= 0 && "OFFSET IS NOT GREATER THAN 0!\n");
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
            assert(LB >= 0 && "LB IS NOT GREATER THAN 0!\n");
        }
        else if (key == "UB") 
        {
            answer = line.substr(3, line.size() - 1);
            DPRINTF(TypeMetadata, "Key: %s Answer: %s\n", key, answer);
            UB = std::stoull(answer);
            assert(UB >= 0 && "UB IS NOT GREATER THAN 0!\n");
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
            
            TypeEntryInfo type_entry_info((uint64_t)OFFSET, 
                                        CORECED,
                                        (uint64_t)LB,
                                        (uint64_t)UB,
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

            // by the time we are here, APSIZE always should be zero! 
            assert(APSIZE == 0 && "METAID is seen and APSIZE is not zero!\n");

            // seperate the information by #
            std::string input = METAID;
            std::istringstream ss(input);
            std::string token;
            std::vector<std::string> tokens;
            while(std::getline(ss, token, '#')) {
                tokens.push_back(token);
            }

            assert(tokens.size() == 8 && "Tokens size is not equal to 8!\n" );

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

            AllocatioPointMeta _AllocPointMeta = AllocatioPointMeta
                                                (
                                                    tokens[0], // FuncName
                                                    ((tokens[1].size() != 0) ? std::stoi(tokens[1]) : 0), // line 
                                                    ((tokens[2].size() != 0) ? std::stoi(tokens[2]) : 0), // column
                                                    (tokens[3]), // TypeName
                                                    ((tokens[4].size() != 0) ? std::stoull(tokens[4]) : 0), // ConstValue
                                                    ((tokens[5].size() != 0) ? std::stoull(tokens[5]) : 0), // Hash1
                                                    ((tokens[6].size() != 0) ? std::stoull(tokens[6]) : 0), // Hash2
                                                    (tokens[7]) //Allocator Name
                                                );
            
            
            
            type_metadata_info.SetAllocationPointMeta(_AllocPointMeta);

            type_metadata_info.SetIsAllocationPointMetadata(true);
            tc->TypeMetaDataBuffer.push_back(type_metadata_info); 


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
        loc = s1.find("TYCHE_TYCHE_SYMS#", 0);
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
        std::string input = sym.second.substr(17, sym.second.size() - 1);
        std::istringstream ss(input);
        std::string token;
        std::vector<std::string> tokens;
        while(std::getline(ss, token, '#')) {
            tokens.push_back(token);
        }

        assert(tokens.size() == 6 && "Tokens size is not equal to 6!\n" );

        AllocatioPointMeta AllocPointMeta = AllocatioPointMeta(
                                                    tokens[0], // FuncName
                                                    ((tokens[4].size() != 0) ? std::stoi(tokens[4]) : 0), // line 
                                                    ((tokens[5].size() != 0) ? std::stoi(tokens[5]) : 0), // column
                                                    (""), // TypeName
                                                    (0), // ConstValue
                                                    ((tokens[2].size() != 0) ? std::stoull(tokens[2]) : 0), // Hash1
                                                    ((tokens[3].size() != 0) ? std::stoull(tokens[3]) : 0), // Hash2
                                                    (tokens[1]) // Allocator Name
                                                );
        tc->AllocationPointMetaBuffer.insert(std::make_pair(sym.first, AllocPointMeta));
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
    assert(sym_info.size());
    assert(sym_name.size());
    assert(sym_shndx.size());
    assert(sym_size.size());


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
            assert("A symbole that starts with _ZTV!\n");
        }
    }

    tc->VirtualTablesBuffer = elf_obj->getVirtualTables();

    return false;
}



// inline static std::ostream& operator<< (std::ostream& out, const TypeMetadataInfo& tmi)
// {
//             assert(tmi.GetValidFlag() && "Printing an invalid TypeMetadataInfo\n");

//             ccprintf(out, "TypeMetadataInfo:" 
//                         "\n\tIsAllocationPointMetadata = %d"
//                         "\n\tFileName = %s"
//                         "\n\tAllocPointMeta = %s"
//                         "\n\tAllocationPointSize = %d" 
//                         "\n\tTypeEntrys = ",
//                         tmi.GetIsAllocationPointMetadata(),
//                         tmi.GetFileName(),
//                         tmi.GetAllocatioPointMeta(),
//                         tmi.GetAllocatioPointMeta()
//                         );
            
//             for (auto const & elem : tmi.GetTypeEntrys())
//             {
//                 ccprintf(out, "\t\tOffset[%d]=\n\t\t%s\n", elem.first, elem.second);
//             }


//             return out;
// }


// inline static std::ostream& operator << (std::ostream& out, const TypeEntryInfo& tei )
// {
//     assert(tei.GetValidFlag() && "Printing an invalid TypeEntryInfo\n");

//             ccprintf(out, "TypeEntryInfo:" 
//                         "\n\tOffset = %d"
//                         "\n\tCoerced = %d"
//                         "\n\tLower Bound = %x"
//                         "\n\tUpper Bound = %x"
//                         "\n\tFlexible Member Array = %d"
//                         "\n\tName = %s"
//                         "\n\tVirtual Table Pointer = %d"
//                         "\n\tMeta Type = %s"
//                         "\n\tParent Type = %s", 
//                         tei.GetOffset(),
//                         tei.GetCoerced(),
//                         tei.GetLowerBound(),
//                         tei.GetUpperBound(),
//                         tei.GetFlexibleMemberArray(),
//                         tei.GetName(),
//                         tei.GetVirtualTablePointer(),
//                         tei.GetMetaType(),
//                         tei.GetParentType()
//                         );
//             return out;
// }


}

