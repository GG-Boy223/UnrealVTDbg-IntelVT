#include "../../Driver.h"
#include "../../poolmanager.h"
#include "../../Globals.h"
#include "InitNtoskrnl.h"


//BOOLEAN InitNtoskrnlSymbolsTable()
//{
//
//    symbolic_access::ModuleExtenderFactory extenderFactory{};
//    const auto& moduleExtender = extenderFactory.Create(L"ntoskrnl.exe");
//    if (!moduleExtender.has_value())
//    {
//        outDebug("ntoskrnl.exe 符号初始化失败..");
//        return FALSE;
//    }
//
//    PsGetNextProcess = (PFN_PSGETNEXTPROCESS)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESS>("PsGetNextProcess");
//
//    return TRUE;
//}

BOOLEAN InitNtoskrnlSymbolsTable()
{
    symbolic_access::ModuleExtenderFactory extenderFactory{};
    const auto& moduleExtender = extenderFactory.Create(L"ntoskrnl.exe");
    if (!moduleExtender.has_value())
    {
        outDebug("ntoskrnl.exe 符号初始化失败。\n");
        return FALSE;
    }

    constexpr size_t InvalidOffset = static_cast<size_t>(0xFFFFFFFF);

    PsGetNextProcess =
        (PFN_PSGETNEXTPROCESS)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESS>("PsGetNextProcess");

    hv::ghv.kprocess_directory_table_base_offset =
        moduleExtender->GetOffset("_KPROCESS", "DirectoryTableBase").value_or(InvalidOffset);

    // 这里沿用旧字段名 kpcr_pcrb_offset，但它的语义要改成 KPCR.CurrentPrcb 的字段偏移。
    hv::ghv.kpcr_pcrb_offset =
        moduleExtender->GetOffset("_KPCR", "CurrentPrcb").value_or(InvalidOffset);

    hv::ghv.kprcb_current_thread_offset =
        moduleExtender->GetOffset("_KPRCB", "CurrentThread").value_or(InvalidOffset);

    //ethread_offset::Cid = moduleExtender->GetOffset("_ETHREAD", "Cid").value_or(InvalidOffset);
    ethread_offset::Cid = InvalidOffset;

    //输出获取的所有符号信息
    outDebug("PsGetNextProcess 地址：%p\n", PsGetNextProcess);
    outDebug("kprocess_directory_table_base_offset 偏移：%p\n", hv::ghv.kprocess_directory_table_base_offset);
    outDebug("kpcr_pcrb_offset 偏移：%p\n", hv::ghv.kpcr_pcrb_offset);
    outDebug("kprcb_current_thread_offset 偏移：%p\n", hv::ghv.kprcb_current_thread_offset);
    //outDebug("解析ethread_offset::Cid: %p\n", ethread_offset::Cid);


    //if (!PsGetNextProcess ||
    //    hv::ghv.kprocess_directory_table_base_offset == InvalidOffset ||
    //    hv::ghv.kpcr_pcrb_offset == InvalidOffset ||
    //    hv::ghv.kprcb_current_thread_offset == InvalidOffset ||
    //    ethread_offset::Cid == InvalidOffset)
    //{
    //    outDebug("InitNtoskrnlSymbolsTable 缺少必需的偏移量。\n");
    //    return FALSE;
    //}

    if (!PsGetNextProcess ||
        hv::ghv.kprocess_directory_table_base_offset == InvalidOffset ||
        hv::ghv.kpcr_pcrb_offset == InvalidOffset ||
        hv::ghv.kprcb_current_thread_offset == InvalidOffset)
    {
        outDebug("VT InitNtoskrnlSymbolsTable 缺少必需的偏移量.\n");
        return FALSE;
    }

    outDebug("InitNtoskrnlSymbolsTable 成功。\n");

    return TRUE;
}
