#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <elf.h>

struct SymbolEntry
{
    uint64_t value;
};

std::map<std::string, SymbolEntry> symbolTable;
std::vector<char> sectionNames;
std::vector<Elf64_Shdr> sectionHeaders; // Declare sectionHeaders globally

void parseObjectFile(const std::string &fileName)
{
    std::ifstream inputFile(fileName, std::ios::binary);

    if (!inputFile)
    {
        std::cerr << "Failed to open input file: " << fileName << std::endl;
        return;
    }

    // Read the ELF header
    Elf64_Ehdr elfHeader;
    inputFile.read(reinterpret_cast<char *>(&elfHeader), sizeof(Elf64_Ehdr));

    // Read the section headers
    sectionHeaders.resize(elfHeader.e_shnum); // Resize sectionHeaders vector
    inputFile.seekg(elfHeader.e_shoff);
    inputFile.read(reinterpret_cast<char *>(sectionHeaders.data()), elfHeader.e_shnum * sizeof(Elf64_Shdr));

    // Read the section name table
    Elf64_Shdr sectionNameTable = sectionHeaders[elfHeader.e_shstrndx];
    sectionNames.resize(sectionNameTable.sh_size);
    inputFile.seekg(sectionNameTable.sh_offset);
    inputFile.read(sectionNames.data(), sectionNameTable.sh_size);

    for (const auto &sectionHeader : sectionHeaders)
    {
        if (sectionHeader.sh_type == SHT_SYMTAB)
        {
            std::vector<Elf64_Sym> symbolEntries(sectionHeader.sh_size / sizeof(Elf64_Sym));
            inputFile.seekg(sectionHeader.sh_offset);
            inputFile.read(reinterpret_cast<char *>(symbolEntries.data()), sectionHeader.sh_size);

            for (const auto &symbolEntry : symbolEntries)
            {
                std::string symbolName = &sectionNames[sectionNameTable.sh_offset] + symbolEntry.st_name;
                SymbolEntry entry;
                entry.value = symbolEntry.st_value;
                symbolTable[symbolName] = entry;
            }
        }
    }

    inputFile.close();
}

void resolveSymbolReferences()
{
    for (auto &symbolEntry : symbolTable)
    {
        if (symbolEntry.second.value == 0)
        {
            const std::string &symbolName = symbolEntry.first;
            auto it = symbolTable.find(symbolName);
            if (it != symbolTable.end())
            {
                symbolEntry.second.value = it->second.value;
            }
        }
    }
}

void performRelocation(const std::string &fileName)
{
    std::fstream file(fileName, std::ios::in | std::ios::out | std::ios::binary);

    if (!file)
    {
        std::cerr << "Failed to open input file: " << fileName << std::endl;
        return;
    }

    // Read the ELF header
    Elf64_Ehdr elfHeader;
    file.read(reinterpret_cast<char *>(&elfHeader), sizeof(Elf64_Ehdr));

    // Perform relocation
    for (auto &sectionHeader : sectionHeaders)
    {
        if (sectionHeader.sh_type == SHT_RELA)
        {
            size_t numRelocations = sectionHeader.sh_size / sizeof(Elf64_Rela);
            std::vector<Elf64_Rela> relocationEntries(numRelocations);
            file.seekg(sectionHeader.sh_offset);
            file.read(reinterpret_cast<char *>(relocationEntries.data()), sectionHeader.sh_size);

            for (auto &relocationEntry : relocationEntries)
            {
                const std::string &symbolName = &sectionNames[sectionHeaders[sectionHeader.sh_link].sh_offset] + ELF64_R_SYM(relocationEntry.r_info);
                auto it = symbolTable.find(symbolName);
                if (it != symbolTable.end())
                {
                    uint64_t symbolValue = it->second.value;
                    uint64_t addend = relocationEntry.r_addend;
                    uint64_t newValue = symbolValue + addend;
                    file.seekp(relocationEntry.r_offset);
                    file.write(reinterpret_cast<const char *>(&newValue), sizeof(newValue));
                }
            }
        }
    }

    file.close();
}

int main()
{
    std::vector<std::string> objectFiles = {"file1.o", "file2.o"};

    // Parse object files and populate symbol table
    for (const auto &objectFile : objectFiles)
    {
        parseObjectFile(objectFile);
    }

    // Resolve symbol references
    resolveSymbolReferences();

    // Perform relocations
    for (const auto &objectFile : objectFiles)
    {
        performRelocation(objectFile);
    }

    // Create output file
    std::ofstream outputFile("combined_sections.txt");

    if (!outputFile)
    {
        std::cerr << "Failed to create output file." << std::endl;
        return 1;
    }

    // Write section information to output file
    for (const auto &sectionHeader : sectionHeaders)
    {
        std::string sectionName = &sectionNames[sectionHeader.sh_name];
        outputFile << "Section Name: " << sectionName << std::endl;
        outputFile << "Section Address: 0x" << std::hex << std::setw(16) << std::setfill('0') << sectionHeader.sh_addr << std::endl;
        outputFile << "Section Offset: 0x" << std::hex << std::setw(16) << std::setfill('0') << sectionHeader.sh_offset << std::endl;
        outputFile << "Section Size: 0x" << std::hex << std::setw(16) << std::setfill('0') << sectionHeader.sh_size << std::endl;
        outputFile << std::endl;
    }

    outputFile.close();

    std::cout << "Combined sections information written to combined_sections.txt" << std::endl;

    return 0;
}
