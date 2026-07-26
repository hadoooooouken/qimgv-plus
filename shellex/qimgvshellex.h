// shellex/qimgvshellex.h
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlobj.h>
#include <thumbcache.h>
#include <shlwapi.h>
#include <string>

// Unique CLSID for QImgvThumbnailProvider COM object
// CLSID: {978A692C-CD23-4A59-8664-98F1E1B9200B}
extern const CLSID CLSID_QImgvThumbnailProvider;

class QImgvThumbnailProvider : public IThumbnailProvider, public IInitializeWithFile, public IInitializeWithStream {
public:
    QImgvThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) noexcept;
    IFACEMETHODIMP_(ULONG) AddRef() noexcept;
    IFACEMETHODIMP_(ULONG) Release() noexcept;

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) noexcept;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode) noexcept;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp,
                               WTS_ALPHATYPE *pdwAlpha) noexcept;

protected:
    virtual ~QImgvThumbnailProvider();

private:
    long m_cRef;
    std::wstring m_szFilePath;
    IStream* m_pStream;
};

class QImgvThumbnailProviderClassFactory : public IClassFactory {
public:
    QImgvThumbnailProviderClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) noexcept;
    IFACEMETHODIMP_(ULONG) AddRef() noexcept;
    IFACEMETHODIMP_(ULONG) Release() noexcept;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid,
                                  void **ppv) noexcept;
    IFACEMETHODIMP LockServer(BOOL fLock) noexcept;

protected:
    virtual ~QImgvThumbnailProviderClassFactory();

private:
    long m_cRef;
};
