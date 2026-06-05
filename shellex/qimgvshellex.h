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

// Unique CLSID for QImgvThumbnailProvider COM object
// CLSID: {978A692C-CD23-4A59-8664-98F1E1B9200B}
extern const CLSID CLSID_QImgvThumbnailProvider;

class QImgvThumbnailProvider : public IThumbnailProvider, public IInitializeWithFile, public IInitializeWithStream {
public:
    QImgvThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode);

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

protected:
    virtual ~QImgvThumbnailProvider();

private:
    long m_cRef;
    wchar_t m_szFilePath[MAX_PATH];
    IStream* m_pStream;
};

class QImgvThumbnailProviderClassFactory : public IClassFactory {
public:
    QImgvThumbnailProviderClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
    IFACEMETHODIMP LockServer(BOOL fLock);

protected:
    virtual ~QImgvThumbnailProviderClassFactory();

private:
    long m_cRef;
};
