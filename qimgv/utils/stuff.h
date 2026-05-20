#pragma once

#include <QString>

#define StdString std::wstring
#define CharType wchar_t

int clamp(int x, int lower, int upper);
int probeOS();
StdString toStdString(QString str);
QString fromStdString(StdString str);
