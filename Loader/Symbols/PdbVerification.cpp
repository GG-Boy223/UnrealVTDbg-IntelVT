#include "SymbolsInternal.h"

namespace
{
    bool LoadPdbFile(std::unique_ptr<char[]>* outData, size_t* outSize)
    {
        if (!outData || !outSize)
        {
            return false;
        }

        std::ifstream f(g_szPdbPath.c_str(), std::ios::binary | std::ios::ate);
        if (!f)
        {
            return false;
        }

        const std::streamoff streamSize = f.tellg();
        if (streamSize <= 0)
        {
            return false;
        }

        const size_t sizeOnDisk = static_cast<size_t>(streamSize);
        std::unique_ptr<char[]> data(new(std::nothrow) char[sizeOnDisk]);
        if (!data)
        {
            return false;
        }

        f.seekg(0, std::ios::beg);
        f.read(data.get(), streamSize);
        const bool readOk = f.gcount() == streamSize;
        f.close();
        if (!readOk)
        {
            return false;
        }

        *outData = std::move(data);
        *outSize = sizeOnDisk;
        return true;
    }

    const char* PdbPage(const char* data, size_t dataSize, size_t pageSize, int pageNumber)
    {
        if (pageNumber < 0 || pageSize == 0)
        {
            return nullptr;
        }

        const size_t page = static_cast<size_t>(pageNumber);
        const size_t offset = page * pageSize;
        if (offset < page || offset >= dataSize || pageSize > dataSize - offset)
        {
            return nullptr;
        }

        return data + offset;
    }
}

// 验证已存在的PDB文件
bool VerifyExistingPdb(const GUID& guid)
{
    return VerifyExistingPdb2(guid, 0);
}

bool VerifyExistingPdb2(const GUID& guid, DWORD age)
{
    std::unique_ptr<char[]> pdbRaw;
    size_t sizeOnDisk = 0;
    if (!LoadPdbFile(&pdbRaw, &sizeOnDisk))
    {
        return false;
    }

    if (sizeOnDisk < sizeof(PDBHeader7))
    {
        return false;
    }

    const PDBHeader7* pHeader = reinterpret_cast<const PDBHeader7*>(pdbRaw.get());
    if (memcmp(pHeader->signature,
        "Microsoft C/C++ MSF 7.00\r\n\x1A""DS\0\0\0",
        sizeof(pHeader->signature)) != 0)
    {
        return false;
    }

    const int pageSizeInt = pHeader->page_size;
    const int filePageCountInt = pHeader->file_page_count;
    const int rootPageListInt = pHeader->root_stream_page_number_list_number;
    if (pageSizeInt <= 0 || filePageCountInt < 0 || rootPageListInt < 0)
    {
        return false;
    }

    const size_t pageSize = static_cast<size_t>(pageSizeInt);
    const size_t filePageCount = static_cast<size_t>(filePageCountInt);
    if (pageSize > sizeOnDisk || filePageCount > sizeOnDisk / pageSize)
    {
        return false;
    }

    const char* rootPageList = PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, rootPageListInt);
    if (!rootPageList)
    {
        return false;
    }

    const size_t rootPageListEntries = (sizeOnDisk - (rootPageList - pdbRaw.get())) /
        sizeof(int);
    if (rootPageListEntries < 1)
    {
        return false;
    }

    const int rootPageNumber = *reinterpret_cast<const int*>(rootPageList);
    const char* rootStream = PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, rootPageNumber);
    if (!rootStream)
    {
        return false;
    }

    const size_t rootStreamBytes = sizeOnDisk - (rootStream - pdbRaw.get());
    if (rootStreamBytes < sizeof(RootStream7))
    {
        return false;
    }

    const RootStream7* pRoot = reinterpret_cast<const RootStream7*>(rootStream);
    if (pRoot->num_streams <= 0)
    {
        return false;
    }

    const size_t numStreams = static_cast<size_t>(pRoot->num_streams);
    if (numStreams > rootStreamBytes / sizeof(int))
    {
        return false;
    }

    std::vector<int> streamSizes;
    streamSizes.reserve(numStreams);
    for (size_t i = 0; i < numStreams; ++i)
    {
        streamSizes.push_back(pRoot->stream_sizes[i]);
    }

    size_t maxRootEntries = (rootStreamBytes - numStreams * sizeof(int)) / sizeof(int);
    size_t pageTableIndex = 0;
    for (size_t i = 0; i < numStreams; ++i)
    {
        int streamSize = streamSizes[i];
        if (streamSize == 0xFFFFFFFF)
        {
            streamSize = 0;
        }
        if (streamSize < 0)
        {
            return false;
        }

        const size_t pageCount = (static_cast<size_t>(streamSize) +
            pageSize - 1) / pageSize;
        if (pageTableIndex > maxRootEntries ||
            pageCount > maxRootEntries - pageTableIndex)
        {
            return false;
        }

        const size_t entriesBase = numStreams + pageTableIndex;
        for (size_t j = 0; j < pageCount; ++j)
        {
            const int pageNumber = pRoot->stream_sizes[entriesBase + j];
            if (PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, pageNumber) == nullptr)
            {
                return false;
            }
        }

        pageTableIndex += pageCount;
    }

    if (numStreams < 2 || streamSizes[1] == 0xFFFFFFFF)
    {
        return false;
    }

    const int guidStreamSize = streamSizes[1];
    if (guidStreamSize < 0 ||
        static_cast<size_t>(guidStreamSize) < sizeof(GUID_StreamData))
    {
        return false;
    }

    const int guidPage = pRoot->stream_sizes[numStreams + 0];
    const char* guidStreamData = PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, guidPage);
    if (!guidStreamData ||
        static_cast<size_t>(guidStreamSize) > sizeOnDisk - (guidStreamData - pdbRaw.get()))
    {
        return false;
    }

    const auto* streamData = reinterpret_cast<const GUID_StreamData*>(guidStreamData);

    // 未启用 age 校验时(历史调用 VerifyExistingPdb),仅比较 GUID。
    if (age == 0)
    {
        return memcmp(&streamData->guid, &guid, sizeof(GUID)) == 0;
    }

    return memcmp(&streamData->guid, &guid, sizeof(GUID)) == 0 &&
        streamData->age == age;
}
