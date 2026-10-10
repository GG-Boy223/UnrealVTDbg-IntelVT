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

    if (numStreams < 4 || streamSizes[1] == 0xFFFFFFFF)
    {
        // 需要 stream 0/2/3 的 page count 才能定位 GUID 流(stream 1)与 DBI 流(stream 3)。
        return false;
    }

    const int guidStreamSize = streamSizes[1];
    if (guidStreamSize < 0 ||
        static_cast<size_t>(guidStreamSize) < sizeof(GUID_StreamData))
    {
        return false;
    }

    const auto stream_page_count = [pageSize](int streamSize) -> size_t
    {
        if (streamSize <= 0) // 0 与 0xFFFFFFFF 都表示空流
        {
            return 0;
        }
        return (static_cast<size_t>(streamSize) + pageSize - 1) / pageSize;
    };

    // MSF 7.00 根流布局：num_streams → 各流 size → 按流顺序拼接的各流 page list，
    // 因此 stream N 的首个 page 位于条目索引 numStreams + Σ(pageCount[0..N-1])。
    //
    // 实测（本机 C:\Symbols\ntkrnlmp.pdb\10EFC2288044B448D2954F042A3396E01）：
    // numStreams=1098、stream0.size=37592(37 页)、stream1.size=93(1 页)；
    // 旧写法固定读 stream_sizes[numStreams + 0]（= stream 0 的首个数据页 4），
    // 而 GUID/info 流(stream 1)的数据页是 8323 —— 合法 PDB 因此永远验证失败。
    const size_t guidPageTableIndex = static_cast<size_t>(numStreams) + stream_page_count(streamSizes[0]);
    const int guidPage = pRoot->stream_sizes[guidPageTableIndex];
    const char* guidStreamData = PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, guidPage);
    if (!guidStreamData ||
        static_cast<size_t>(guidStreamSize) > sizeOnDisk - (guidStreamData - pdbRaw.get()))
    {
        return false;
    }

    const auto* streamData = reinterpret_cast<const GUID_StreamData*>(guidStreamData);
    if (memcmp(&streamData->guid, &guid, sizeof(GUID)) != 0)
    {
        return false;
    }

    // 未启用 age 校验时(历史调用 VerifyExistingPdb),仅比较 GUID。
    if (age == 0)
    {
        return true;
    }

    // age 必须取自 DBI 流(stream 3)的 Age 字段：PE 的 RSDS.Age 等于 DBI.Age，
    // 而 GUID 流(stream 1)的第三个字段并不等于 RSDS age
    // （同机实测：info=5、DBI=1、RSDS=1）。旧逻辑用 info 流字段比较 age，必然失败。
    const size_t dbiPageTableIndex = static_cast<size_t>(numStreams) +
        stream_page_count(streamSizes[0]) +
        stream_page_count(streamSizes[1]) +
        stream_page_count(streamSizes[2]);
    const int dbiPage = pRoot->stream_sizes[dbiPageTableIndex];
    const char* dbiStreamData = PdbPage(pdbRaw.get(), sizeOnDisk, pageSize, dbiPage);
    constexpr size_t kDbiAgeOffset = sizeof(int) * 2; // Signature / Version / Age
    if (!dbiStreamData || streamSizes[3] < static_cast<int>(kDbiAgeOffset + sizeof(int)))
    {
        return false;
    }

    const int dbiAge = *reinterpret_cast<const int*>(dbiStreamData + kDbiAgeOffset);
    return dbiAge >= 0 && static_cast<DWORD>(dbiAge) == age;
}
