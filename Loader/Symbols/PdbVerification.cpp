#include "SymbolsInternal.h"

bool VerifyExistingPdb(const GUID& guid)
{
    std::ifstream f(g_szPdbPath.c_str(), std::ios::binary | std::ios::ate);
    if (f.bad())
    {
        return false;
    }

    size_t size_on_disk = static_cast<size_t>(f.tellg());
    if (!size_on_disk)
    {
        f.close();

        return false;
    }

    char* pdb_raw = new char[size_on_disk];
    if (!pdb_raw)
    {
        f.close();

        return false;
    }

    f.seekg(std::ios::beg);
    f.read(pdb_raw, size_on_disk);
    f.close();

    if (size_on_disk < sizeof(PDBHeader7))
    {
        delete[] pdb_raw;

        return false;
    }

    auto* pPDBHeader = reinterpret_cast<PDBHeader7*>(pdb_raw);

    if (memcmp(pPDBHeader->signature, "Microsoft C/C++ MSF 7.00\r\n\x1A""DS\0\0\0", sizeof(PDBHeader7::signature)))
    {
        delete[] pdb_raw;

        return false;
    }

    if (size_on_disk < (size_t)pPDBHeader->page_size * pPDBHeader->file_page_count)
    {
        delete[] pdb_raw;

        return false;
    }

    int* pRootPageNumber = reinterpret_cast<int*>(pdb_raw + (size_t)pPDBHeader->root_stream_page_number_list_number * pPDBHeader->page_size);
    auto* pRootStream = reinterpret_cast<RootStream7*>(pdb_raw + (size_t)(*pRootPageNumber) * pPDBHeader->page_size);

    std::map<int, std::vector<int>> streams;
    int current_page_number = 0;

    for (int i = 0; i != pRootStream->num_streams; ++i)
    {
        int current_size = pRootStream->stream_sizes[i] == 0xFFFFFFFF ? 0 : pRootStream->stream_sizes[i];

        int current_page_count = current_size / pPDBHeader->page_size;
        if (current_size % pPDBHeader->page_size)
        {
            ++current_page_count;
        }

        std::vector<int> numbers;

        for (int j = 0; j != current_page_count; ++j, ++current_page_number)
        {
            numbers.push_back(pRootStream->stream_sizes[pRootStream->num_streams + current_page_number]);
        }

        streams.insert({ i, numbers });
    }

    auto pdb_info_page_index = streams.at(1).at(0);

    auto* stram_data = reinterpret_cast<GUID_StreamData*>(pdb_raw + (size_t)(pdb_info_page_index) * pPDBHeader->page_size);

    int guid_eq = memcmp(&stram_data->guid, &guid, sizeof(GUID));

    delete[] pdb_raw;

    return (guid_eq == 0);
}

bool VerifyExistingPdb2(const GUID& guid, DWORD age)
{
    std::ifstream f(g_szPdbPath.c_str(), std::ios::binary | std::ios::ate);
    if (f.bad())
    {
        return false;
    }

    size_t size_on_disk = static_cast<size_t>(f.tellg());
    f.close();
    if (!size_on_disk)
    {
        return false;
    }

    std::unique_ptr<char[]> pdb_raw(new(std::nothrow) char[size_on_disk]);
    if (!pdb_raw)
    {
        return false;
    }

    f.open(g_szPdbPath.c_str(), std::ios::binary);
    f.read(pdb_raw.get(), size_on_disk);
    f.close();

    if (size_on_disk < sizeof(PDBHeader7))
    {
        return false;
    }

    auto* pPDBHeader = reinterpret_cast<PDBHeader7*>(pdb_raw.get());
    if (memcmp(pPDBHeader->signature,
        "Microsoft C/C++ MSF 7.00\r\n\x1A""DS\0\0\0",
        sizeof(pPDBHeader->signature)) != 0)
    {
        return false;
    }

    if (size_on_disk < static_cast<size_t>(pPDBHeader->page_size) *
        pPDBHeader->file_page_count)
    {
        return false;
    }

    int* pRootPageNumber = reinterpret_cast<int*>(
        pdb_raw.get() +
        static_cast<size_t>(pPDBHeader->root_stream_page_number_list_number) *
        pPDBHeader->page_size);

    auto* pRootStream = reinterpret_cast<RootStream7*>(
        pdb_raw.get() +
        static_cast<size_t>(*pRootPageNumber) * pPDBHeader->page_size);

    std::vector<std::vector<int>> streams(pRootStream->num_streams);
    int current_page_number = 0;
    for (int i = 0; i < pRootStream->num_streams; ++i)
    {
        int sz = pRootStream->stream_sizes[i];
        if (sz == 0xFFFFFFFF)
        {
            sz = 0;
        }
        int pages = (sz + pPDBHeader->page_size - 1) / pPDBHeader->page_size;
        streams[i].reserve(pages);
        for (int j = 0; j < pages; ++j, ++current_page_number)
        {
            streams[i].push_back(
                pRootStream->stream_sizes[pRootStream->num_streams + current_page_number]);
        }
    }

    if (streams.size() < 2 || streams[1].empty())
    {
        return false;
    }
    auto* stram_data = reinterpret_cast<GUID_StreamData*>(
        pdb_raw.get() +
        static_cast<size_t>(streams[1][0]) * pPDBHeader->page_size);

    bool guidOk = (memcmp(&stram_data->guid, &guid, sizeof(GUID)) == 0);
    bool ageOk = (stram_data->age == age);

    return guidOk && ageOk;
}
