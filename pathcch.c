#define STATIC_PATHCCH
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <strsafe.h>
#include <PathCch.h>

#ifdef __cplusplus
#define NULLPTR nullptr
#else
#include <stdbool.h>
#define NULLPTR NULL
#endif

#define WINPATHCCHWORKERAPI static __inline bool __fastcall

#define EXTENDED_PATH_PREFIX L"\\\\?\\"
#define EXTENDED_PATH_PREFIX_LEN (ARRAYSIZE(EXTENDED_PATH_PREFIX) - 1)

#define UNC_PREFIX L"\\\\?\\UNC\\"
#define UNC_PREFIX_LEN (ARRAYSIZE(UNC_PREFIX) - 1)

#define PATHCCH_FORCE_ENABLE_DISABLE_LONG_NAME_PROCESS (PATHCCH_FORCE_ENABLE_LONG_NAME_PROCESS | PATHCCH_FORCE_DISABLE_LONG_NAME_PROCESS)
#define PATHCCH_FORCE_DISABLE_LONG_PATHS (PATHCCH_ALLOW_LONG_PATHS | PATHCCH_FORCE_DISABLE_LONG_NAME_PROCESS)

typedef BOOLEAN(NTAPI *PFN_RTLARELONGPATHSENABLED)(void);


WINPATHCCHWORKERAPI AreLongPathsEnabled(void)
{
    static PVOID fpRtlAreLongPathsEnabled;
    HMODULE hModule;
    PVOID fp;
    PVOID tp;

    fp = InterlockedCompareExchangePointer(&fpRtlAreLongPathsEnabled, NULLPTR, NULLPTR);
    if ( !fp ) {
        hModule = GetModuleHandleW(L"ntdll.dll");
        if ( !hModule )
            return false;

        fp = GetProcAddress(hModule, "RtlAreLongPathsEnabled");
        if ( !fp )
            return false;

        tp = InterlockedCompareExchangePointer(&fpRtlAreLongPathsEnabled, fp, NULLPTR);
        if ( tp )
            fp = tp;
    }
    return ((PFN_RTLARELONGPATHSENABLED)fp)();
}


WINPATHCCHWORKERAPI IsValidExtension(PCWSTR pszExt)
{
    size_t cchExt;

    if ( !pszExt )
        return false;

    cchExt = wcslen(pszExt);

    return (cchExt < _MAX_EXT || cchExt == _MAX_EXT && *pszExt == '.')
        && !wcschr(pszExt, ' ')
        && !wcschr(pszExt, '\\')
        && (!cchExt || !wcschr(pszExt + 1, '.'));
}


WINPATHCCHWORKERAPI IsExtendedLengthDosDevicePath(PCWSTR pszPath)
{
    return !wcsncmp(pszPath, EXTENDED_PATH_PREFIX, EXTENDED_PATH_PREFIX_LEN);
}


WINPATHCCHWORKERAPI StringIsGUID(PCWSTR pszString)
{
    const WCHAR s[] = L"{00000000-0000-0000-0000-000000000000}";
    WCHAR c;

    for ( size_t i = 0; i < ARRAYSIZE(s) - 1; ++i ) {
        c = pszString[i];
        if ( c != s[i] && (s[i] != '0' || (c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f')) )
            return false;
    }
    return true;
}


WINPATHCCHWORKERAPI PathIsVolumeGUID(PCWSTR pszPath)
{
    return !_wcsnicmp(pszPath, VOLUME_PREFIX, VOLUME_PREFIX_LEN)
        && StringIsGUID(pszPath + VOLUME_PREFIX_LEN);
}


WINPATHCCHWORKERAPI AreOptionsValidAndOptInLongPathAwareProcess(ULONG *pdwFlags)
{
    ULONG Flags = *pdwFlags;

    if ( Flags & PATHCCH_ALLOW_LONG_PATHS ) {
        if ( (Flags & PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH) )
            return false;

        if ( Flags & PATHCCH_FORCE_ENABLE_DISABLE_LONG_NAME_PROCESS )
            return (Flags & PATHCCH_FORCE_ENABLE_DISABLE_LONG_NAME_PROCESS) != PATHCCH_FORCE_ENABLE_DISABLE_LONG_NAME_PROCESS;

        *pdwFlags |= AreLongPathsEnabled()
            ? PATHCCH_FORCE_ENABLE_LONG_NAME_PROCESS
            : PATHCCH_FORCE_DISABLE_LONG_NAME_PROCESS;
        return true;
    }
    return Flags & PATHCCH_FORCE_ENABLE_DISABLE_LONG_NAME_PROCESS;
}


WINPATHCCHWORKERAPI ExtraSpaceIfNeeded(ULONG dwFlags, size_t *pcch)
{
    if ( (dwFlags & PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH)
        || (*pcch > MAX_PATH && (dwFlags & PATHCCH_FORCE_DISABLE_LONG_PATHS) == PATHCCH_FORCE_DISABLE_LONG_PATHS) ) {
        *pcch += EXTENDED_PATH_PREFIX_LEN;
        return true;
    }
    return false;
}


#pragma warning( push )
#pragma warning( disable : 6387 28196 )
WINPATHCCHAPI BOOL APIENTRY PathIsUNCEx(
    _In_ PCWSTR pszPath,
    _Outptr_opt_ PCWSTR *ppszServer)
{
    if ( ppszServer )
        *ppszServer = NULLPTR;

    if ( !_wcsnicmp(pszPath, UNC_PREFIX, UNC_PREFIX_LEN) ) {
        if ( ppszServer )
            *ppszServer = pszPath + UNC_PREFIX_LEN;

        return TRUE;
    }
    return FALSE;
}
#pragma warning( pop )


WINPATHCCHAPI BOOL APIENTRY PathCchIsRoot(
    _In_opt_ PCWSTR pszPath)
{
    const WCHAR str[] = L":\\";
    const size_t len = ARRAYSIZE(str) - 1;
    PCWSTR pszServer;

    if ( !pszPath || !*pszPath )
        return FALSE;

    if ( (iswalpha(*pszPath) && !_wcsnicmp(pszPath + 1, str, len))
        || (*pszPath == '\\' && !pszPath[1]) ) {
        return TRUE;
    }
    if ( PathIsUNCEx(pszPath, &pszServer) ) {
        for ( bool flag = false; *pszServer; ++pszServer ) {
            if ( *pszServer == '\\' ) {
                if ( flag )
                    return FALSE;
                flag = true;
            }
        }
        return TRUE;
    }
    if ( IsExtendedLengthDosDevicePath(pszPath) ) {
        pszServer = pszPath + EXTENDED_PATH_PREFIX_LEN;
        if ( iswalpha(*pszServer++) && !_wcsnicmp(pszServer, str, len) && !pszServer[len] )
            return true;

        pszServer = pszPath + VOLUME_PREFIX_LEN + 38;
        if ( PathIsVolumeGUID(pszPath) && *pszServer == '\\' && !pszServer[1] )
            return true;
    }
    return false;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchAddBackslashEx(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath,
    _Outptr_opt_result_buffer_(*pcchRemaining) PWSTR *ppszEnd,
    _Out_opt_ size_t *pcchRemaining)
{
    size_t len;
    size_t cchRemaining;
    HRESULT hr = S_FALSE;
    PWSTR pszEnd;

    if ( ppszEnd )
        *ppszEnd = NULLPTR;
    if ( pcchRemaining )
        *pcchRemaining = 0;

    len = wcslen(pszPath);
    if ( len >= cchPath )
        return STRSAFE_E_INSUFFICIENT_BUFFER;

    cchRemaining = cchPath - len;
    pszEnd = pszPath + len;
    if ( len && pszPath[len - 1] != '\\' ) {
        hr = StringCchCopyExW(pszEnd, cchRemaining, L"\\", &pszEnd, &cchRemaining, 0);
        if ( FAILED(hr) )
            return hr;
    }

    if ( ppszEnd )
        *ppszEnd = pszEnd;
    if ( pcchRemaining )
        *pcchRemaining = cchRemaining;
    return hr;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchAddBackslash(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath)
{
    return PathCchAddBackslashEx(pszPath, cchPath, NULLPTR, NULLPTR);
}


#pragma warning( push )
#pragma warning( disable : 6387 28196 )
WINPATHCCHAPI HRESULT APIENTRY PathCchRemoveBackslashEx(
    _Inout_updates_(_Inexpressible_(cchPath)) PWSTR pszPath,
    _In_ size_t cchPath,
    _Outptr_opt_result_buffer_(*pcchRemaining) PWSTR *ppszEnd,
    _Out_opt_ size_t *pcchRemaining)
{
    size_t len;

    if ( ppszEnd )
        *ppszEnd = NULLPTR;
    if ( pcchRemaining )
        *pcchRemaining = 0;

    len = wcslen(pszPath);
    if ( len >= cchPath )
        return E_INVALIDARG;

    if ( !len || pszPath[len - 1] != '\\' || PathCchIsRoot(pszPath) )
        return S_FALSE;

    pszPath[--len] = '\0';

    if ( ppszEnd )
        *ppszEnd = pszPath + len;
    if ( pcchRemaining )
        *pcchRemaining = cchPath - len;
    return S_OK;
}
#pragma warning( pop )


WINPATHCCHAPI HRESULT APIENTRY PathCchRemoveBackslash(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath)
{
    return PathCchRemoveBackslashEx(pszPath, cchPath, NULLPTR, NULLPTR);
}


WINPATHCCHAPI HRESULT APIENTRY PathCchSkipRoot(
    _In_ PCWSTR pszPath,
    _Outptr_ PCWSTR *ppszRootEnd)
{
    PCWSTR pszServer;

    if ( !pszPath || !*pszPath || !ppszRootEnd )
        return E_INVALIDARG;

    *ppszRootEnd = NULLPTR;

    if ( PathIsUNCEx(pszPath, &pszServer) ) {
        for ( bool flag = false; *pszServer; ++pszServer ) {
            if ( *pszServer == '\\' ) {
                if ( flag )
                    break;
                flag = true;
            }
        }
        *ppszRootEnd = pszServer;
        return S_OK;
    }
    if ( *pszPath == '\\' && pszPath[1] != '\\' ) {
        *ppszRootEnd = pszPath + 1;
        return S_OK;
    }
    if ( PathIsVolumeGUID(pszPath) ) {
        pszPath += VOLUME_PREFIX_LEN;
    } else {
        if ( IsExtendedLengthDosDevicePath(pszPath) )
            pszPath += EXTENDED_PATH_PREFIX_LEN;

        if ( !iswalpha(*pszPath++) || *pszPath++ != ':' )
            return E_INVALIDARG;
    }
    *ppszRootEnd = (*pszPath == '\\') ? pszPath + 1 : pszPath;
    return S_OK;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchStripToRoot(
    _Inout_updates_(_Inexpressible_(cchPath)) PWSTR pszPath,
    _In_ size_t cchPath)
{
    HRESULT hr;
    PWSTR pszRootEnd;
    bool flag;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH )
        return E_INVALIDARG;

    hr = PathCchSkipRoot(pszPath, &pszRootEnd);
    if ( SUCCEEDED(hr) ) {
        if ( pszRootEnd >= pszPath + cchPath ) {
            flag = !!*pszRootEnd;
            if ( flag )
                *pszRootEnd = '\0';

            hr = PathCchRemoveBackslash(pszPath, cchPath);
            if ( SUCCEEDED(hr) )
                return flag ? S_OK : hr;
        } else {
            hr = E_INVALIDARG;
        }
    }
    *pszPath = '\0';
    return hr;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchRemoveFileSpec(
    _Inout_updates_(_Inexpressible_(cchPath)) PWSTR pszPath,
    _In_ size_t cchPath)
{
    PWSTR pszFileSpec;
    PWSTR pszEnd;
    PWSTR tmp;
    bool flag;
    HRESULT hr;

    if ( !pszPath || cchPath > PATHCCH_MAX_CCH )
        return E_INVALIDARG;

    if ( FAILED(PathCchSkipRoot(pszPath, &pszFileSpec)) )
        pszFileSpec = pszPath;

    pszEnd = pszPath + cchPath;

    if ( pszFileSpec >= pszEnd )
        return E_INVALIDARG;

    tmp = wcsrchr(pszPath, '\\');
    if ( tmp ) {
        if ( tmp >= pszEnd )
            return E_INVALIDARG;
        pszFileSpec = tmp;
    }
    flag = !!*pszFileSpec;
    if ( flag )
        *pszFileSpec = '\0';

    hr = PathCchRemoveBackslash(pszPath, cchPath);
    return (SUCCEEDED(hr) && flag) ? S_OK : hr;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchFindExtension(
    _In_reads_(_Inexpressible_(cchPath)) PCWSTR pszPath,
    _In_ size_t cchPath,
    _Outptr_ PCWSTR *ppszExt)
{
    PCWSTR pszEnd;
    PCWSTR pszExt = NULLPTR;

    *ppszExt = NULLPTR;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH )
        return E_INVALIDARG;

    pszEnd = pszPath + cchPath;

    if ( pszPath >= pszEnd )
        return E_INVALIDARG;

    do {
        if ( !*pszPath )
            break;

        switch ( *pszPath ) {
        case '.':
            pszExt = pszPath;
            break;
        case '\\':
        case ' ':
            pszExt = NULLPTR;
            break;
        }
        ++pszPath;
    } while ( pszPath < pszEnd );

    if ( pszPath >= pszEnd )
        return E_INVALIDARG;

    *ppszExt = pszExt ? pszExt : pszPath;
    return S_OK;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchAddExtension(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath,
    _In_ PCWSTR pszExt)
{
    bool flag;
    HRESULT hr;
    PWSTR pszDest;
    size_t cchRemaining;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH || !IsValidExtension(pszExt) )
        return E_INVALIDARG;

    flag = IsExtendedLengthDosDevicePath(pszPath);
    if ( !flag && cchPath > MAX_PATH )
        cchPath = MAX_PATH;

    hr = PathCchFindExtension(pszPath, cchPath, &pszDest);
    if ( FAILED(hr) )
        return hr;

    if ( *pszDest )
        return S_FALSE;

    if ( !*pszExt || *pszExt == '.' && !pszExt[1] )
        return S_OK;

    cchRemaining = (cchPath - (pszDest - pszPath));
    if ( *pszExt != '.' )
        hr = StringCchCopyExW(pszDest, cchRemaining, L".", &pszDest, &cchRemaining, 0);

    if ( SUCCEEDED(hr) )
        hr = StringCchCopyW(pszDest, cchRemaining, pszExt);

    if ( hr != STRSAFE_E_INSUFFICIENT_BUFFER )
        return hr;

    *pszDest = '\0';
    if ( flag ) {
        if ( cchPath != PATHCCH_MAX_CCH )
            return hr;
    } else if ( cchPath != MAX_PATH ) {
        return hr;
    }
    return CO_E_PATHTOOLONG;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchRenameExtension(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath,
    _In_ PCWSTR pszExt)
{
    bool flag;
    HRESULT hr;
    PWSTR pszDest;
    size_t cchRemaining;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH || !IsValidExtension(pszExt) )
        return E_INVALIDARG;

    flag = IsExtendedLengthDosDevicePath(pszPath);
    if ( !flag && cchPath > MAX_PATH )
        cchPath = MAX_PATH;

    hr = PathCchFindExtension(pszPath, cchPath, &pszDest);
    if ( FAILED(hr) )
        return hr;

    if ( !*pszExt || (*pszExt == '.' && !pszExt[1]) ) {
        *pszDest = '\0';
        return S_OK;
    }
    cchRemaining = cchPath - (pszDest - pszPath);
    if ( *pszExt != '.' )
        hr = StringCchCopyExW(pszDest, cchRemaining, L".", &pszDest, &cchRemaining, 0);

    if ( SUCCEEDED(hr) )
        hr = StringCchCopyW(pszDest, cchRemaining, pszExt);

    if ( hr != STRSAFE_E_INSUFFICIENT_BUFFER )
        return hr;

    if ( flag ) {
        if ( cchPath != PATHCCH_MAX_CCH )
            return hr;
    } else if ( cchPath != MAX_PATH ) {
        return hr;
    }
    return CO_E_PATHTOOLONG;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchRemoveExtension(
    _Inout_updates_(_Inexpressible_(cchPath)) PWSTR pszPath,
    _In_ size_t cchPath)
{
    HRESULT hr;
    PWSTR pszExt;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH )
        return E_INVALIDARG;

    if ( !IsExtendedLengthDosDevicePath(pszPath) && cchPath > MAX_PATH )
        cchPath = MAX_PATH;

    hr = PathCchFindExtension(pszPath, cchPath, &pszExt);
    if ( FAILED(hr) )
        return hr;

    if ( *pszExt ) {
        *pszExt = '\0';
        return S_OK;
    }
    return S_FALSE;
}


/* PATHCCH_OPTIONS */WINPATHCCHAPI HRESULT APIENTRY PathCchCanonicalizeEx(
    _Out_writes_(cchPathOut) PWSTR pszPathOut,
    _In_ size_t cchPathOut,
    _In_ PCWSTR pszPathIn,
    _In_ ULONG dwFlags)
{
    return E_NOTIMPL;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchCanonicalize(
    _Out_writes_(cchPathOut) PWSTR pszPathOut,
    _In_ size_t cchPathOut,
    _In_ PCWSTR pszPathIn)
{
    return PathCchCanonicalizeEx(pszPathOut, cchPathOut, pszPathIn, 0);
}


/* PATHCCH_OPTIONS */WINPATHCCHAPI HRESULT APIENTRY PathCchCombineEx(
    _Out_writes_(cchPathOut) PWSTR pszPathOut,
    _In_ size_t cchPathOut,
    _In_opt_ PCWSTR pszPathIn,
    _In_opt_ PCWSTR pszMore,
    _In_ ULONG dwFlags)
{
    return E_NOTIMPL;
}


WINPATHCCHAPI HRESULT APIENTRY PathCchCombine(
    _Out_writes_(cchPathOut) PWSTR pszPathOut,
    _In_ size_t cchPathOut,
    _In_opt_ PCWSTR pszPathIn,
    _In_opt_ PCWSTR pszMore)
{
    return PathCchCombineEx(pszPathOut, cchPathOut, pszPathIn, pszMore, 0);
}


/* PATHCCH_OPTIONS */WINPATHCCHAPI HRESULT APIENTRY PathCchAppendEx(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath,
    _In_opt_ PCWSTR pszMore,
    _In_ ULONG dwFlags)
{
    if ( !pszMore )
        return PathCchCombineEx(pszPath, cchPath, pszPath, pszMore, dwFlags);

    if ( IsExtendedLengthDosDevicePath(pszMore) ) {
        if ( PathIsUNCEx(pszMore, NULLPTR) )
            return PathCchCombineEx(pszPath, cchPath, pszPath, pszMore, dwFlags);
    } else {
        while ( *pszMore == '\\' )
            ++pszMore;
    }
    return PathCchCombineEx(pszPath, cchPath, pszPath, pszMore, dwFlags);
}


WINPATHCCHAPI HRESULT APIENTRY PathCchAppend(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath,
    _In_opt_ PCWSTR pszMore)
{
    return PathCchAppendEx(pszPath, cchPath, pszMore, 0);
}


WINPATHCCHAPI HRESULT APIENTRY PathCchStripPrefix(
    _Inout_updates_(cchPath) PWSTR pszPath,
    _In_ size_t cchPath)
{
    PCWSTR pszSrc;

    if ( !pszPath || !cchPath || cchPath > PATHCCH_MAX_CCH )
        return E_INVALIDARG;

    if ( !IsExtendedLengthDosDevicePath(pszPath) )
        return S_FALSE;

    if ( cchPath < EXTENDED_PATH_PREFIX_LEN )
        return E_INVALIDARG;

    if ( PathIsUNCEx(pszPath, &pszSrc) ) {
        if ( cchPath < UNC_PREFIX_LEN )
            return E_INVALIDARG;
    } else {
        pszSrc = pszPath + EXTENDED_PATH_PREFIX_LEN;
        if ( !iswalpha(*pszSrc) || pszSrc[1] != ':' )
            return FALSE;
    }
    return StringCchCopyW(pszPath, cchPath, pszSrc);
}


/* PATHCCH_OPTIONS */WINPATHCCHAPI HRESULT APIENTRY PathAllocCombine(
    _In_opt_ PCWSTR pszPathIn,
    _In_opt_ PCWSTR pszMore,
    _In_ ULONG dwFlags,
    _Outptr_ PWSTR *ppszPathOut)
{
    size_t cchPathIn;
    size_t cchMore;
    PCWCHAR pLastChar = NULLPTR;
    size_t cchPathOut = 0;
    size_t cchMax = (dwFlags & (PATHCCH_ALLOW_LONG_PATHS | PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH)) ? PATHCCH_MAX_CCH : MAX_PATH;
    PWSTR pszPathOut;
    HRESULT hr;

    if ( !ppszPathOut )
        return E_INVALIDARG;

    *ppszPathOut = NULLPTR;

    if ( !AreOptionsValidAndOptInLongPathAwareProcess(&dwFlags) || !pszPathIn && !pszMore )
        return E_INVALIDARG;

    if ( pszPathIn ) {
        cchPathIn = wcslen(pszPathIn);
        if ( cchPathIn >= PATHCCH_MAX_CCH )
            return CO_E_PATHTOOLONG;
        if ( cchPathIn ) {
            pLastChar = pszPathIn + cchPathIn - 1;
            cchPathOut += cchPathIn + 1;
        }
    }
    if ( pszMore ) {
        cchMore = wcslen(pszMore);
        if ( cchMore >= PATHCCH_MAX_CCH )
            return CO_E_PATHTOOLONG;
        if ( cchMore ) {
            pLastChar = pszMore + cchMore - 1;
            cchPathOut += cchMore + 1;
        }
    }
    if ( (dwFlags & PATHCCH_ENSURE_TRAILING_SLASH) && pLastChar && *pLastChar != '\\' )
        ++cchPathOut;

    if ( !cchPathOut )
        cchPathOut = 2;

    ExtraSpaceIfNeeded(dwFlags, &cchPathOut);

    if ( cchPathOut > cchMax )
        cchPathOut = cchMax;

    pszPathOut = (PWSTR)LocalAlloc(LMEM_ZEROINIT, cchPathOut * (sizeof *pszPathOut));
    if ( !pszPathOut )
        return E_OUTOFMEMORY;

    hr = PathCchCombineEx(pszPathOut, cchPathOut, pszPathIn, pszMore, dwFlags);
    if ( FAILED(hr) ) {
        LocalFree(pszPathOut);
        return hr;
    }
    *ppszPathOut = pszPathOut;
    return S_OK;
}


/* PATHCCH_OPTIONS */WINPATHCCHAPI HRESULT APIENTRY PathAllocCanonicalize(
    _In_ PCWSTR pszPathIn,
    _In_ ULONG dwFlags,
    _Outptr_ PWSTR *ppszPathOut)
{
    PWSTR pszPathOut;
    HRESULT hr;
    size_t cchPathIn;
    size_t cchPathOut = 0;
    size_t cchMax = (dwFlags & (PATHCCH_ALLOW_LONG_PATHS | PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH)) ? PATHCCH_MAX_CCH : MAX_PATH;

    if ( !ppszPathOut )
        return E_INVALIDARG;

    *ppszPathOut = NULLPTR;

    if ( !AreOptionsValidAndOptInLongPathAwareProcess(&dwFlags) )
        return E_INVALIDARG;

    cchPathIn = wcslen(pszPathIn);
    if ( cchPathIn >= PATHCCH_MAX_CCH )
        return CO_E_PATHTOOLONG;
    if ( cchPathIn ) {
        cchPathOut = cchPathIn + 1;

        if ( (dwFlags & PATHCCH_ENSURE_TRAILING_SLASH) && pszPathIn[cchPathIn - 1] != '\\' )
            ++cchPathOut;
    }
    if ( !cchPathOut )
        cchPathOut = 2;

    ExtraSpaceIfNeeded(dwFlags, &cchPathOut);

    if ( cchPathOut > cchMax )
        cchPathOut = cchMax;

    pszPathOut = (PWSTR)LocalAlloc(LMEM_ZEROINIT, cchPathOut * (sizeof *pszPathOut));
    if ( !pszPathOut )
        return E_OUTOFMEMORY;

    hr = PathCchCanonicalizeEx(pszPathOut, cchPathOut, pszPathIn, dwFlags);
    if ( FAILED(hr) ) {
        LocalFree(pszPathOut);
        return hr;
    }
    *ppszPathOut = pszPathOut;
    return S_OK;
}
