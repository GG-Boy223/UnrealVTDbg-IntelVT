#pragma once

#ifndef _LOG_H

int outDebug(const TCHAR* _Format, ...);

//报告严重错误
void ReportSeriousError(LPCSTR lpText);

#endif // !_LOG_H
