#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <thumbcache.h>
#include <wincodec.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace {

// {5D628F91-1171-4FE5-97C0-B10115B92058}
constexpr CLSID kDom3DThumbnailProviderClsid = {
    0x5d628f91,
    0x1171,
    0x4fe5,
    {0x97, 0xc0, 0xb1, 0x01, 0x15, 0xb9, 0x20, 0x58}
};

constexpr wchar_t kClsidString[] = L"{5D628F91-1171-4FE5-97C0-B10115B92058}";
constexpr wchar_t kThumbnailHandlerClsid[] = L"{E357FCCD-A995-4576-B01F-234630154E96}";
constexpr wchar_t kProgId[] = L"Dom3D.Project";

HMODULE g_module = nullptr;
long g_dll_refs = 0;

template <typename T>
void safe_release(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

std::vector<std::uint8_t> read_stream(IStream* stream) {
    std::vector<std::uint8_t> data;
    if (!stream) {
        return data;
    }

    STATSTG stat{};
    if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0) {
        const auto size = static_cast<size_t>(stat.cbSize.QuadPart);
        data.resize(size);
        ULONG read = 0;
        if (FAILED(stream->Read(data.data(), static_cast<ULONG>(data.size()), &read))) {
            return {};
        }
        data.resize(read);
        return data;
    }

    std::uint8_t buffer[4096]{};
    for (;;) {
        ULONG read = 0;
        if (FAILED(stream->Read(buffer, sizeof(buffer), &read)) || read == 0) {
            break;
        }
        data.insert(data.end(), buffer, buffer + read);
    }
    return data;
}

std::string extract_thumbnail_base64(const std::vector<std::uint8_t>& file_data) {
    const std::string xml(file_data.begin(), file_data.end());
    const std::string tag = "<thumbnail";
    const size_t tag_pos = xml.find(tag);
    if (tag_pos == std::string::npos) {
        return {};
    }

    const size_t content_start = xml.find('>', tag_pos);
    if (content_start == std::string::npos) {
        return {};
    }

    const size_t content_end = xml.find("</thumbnail>", content_start + 1);
    if (content_end == std::string::npos || content_end <= content_start + 1) {
        return {};
    }

    std::string base64 = xml.substr(content_start + 1, content_end - content_start - 1);
    base64.erase(std::remove_if(base64.begin(), base64.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), base64.end());
    return base64;
}

std::vector<std::uint8_t> decode_base64(const std::string& base64) {
    if (base64.empty()) {
        return {};
    }

    DWORD decoded_size = 0;
    if (!CryptStringToBinaryA(base64.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &decoded_size, nullptr, nullptr)
        || decoded_size == 0) {
        return {};
    }

    std::vector<std::uint8_t> decoded(decoded_size);
    if (!CryptStringToBinaryA(base64.c_str(), 0, CRYPT_STRING_BASE64, decoded.data(), &decoded_size, nullptr, nullptr)) {
        return {};
    }
    decoded.resize(decoded_size);
    return decoded;
}

HRESULT create_hbitmap_from_png(const std::vector<std::uint8_t>& png_data, UINT requested_size, HBITMAP* bitmap) {
    if (!bitmap || png_data.empty()) {
        return E_INVALIDARG;
    }
    *bitmap = nullptr;

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* converted = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICBitmapSource* source = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return hr;
    }

    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(const_cast<BYTE*>(png_data.data()), static_cast<DWORD>(png_data.size()));
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr)) {
        hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, frame, &converted);
    }

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr)) {
        hr = converted->GetSize(&width, &height);
    }

    if (SUCCEEDED(hr) && requested_size > 0 && (width > requested_size || height > requested_size)) {
        const double scale = std::min(static_cast<double>(requested_size) / width,
                                      static_cast<double>(requested_size) / height);
        const UINT scaled_width = std::max<UINT>(1, static_cast<UINT>(width * scale));
        const UINT scaled_height = std::max<UINT>(1, static_cast<UINT>(height * scale));
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(converted, scaled_width, scaled_height, WICBitmapInterpolationModeFant);
        }
        if (SUCCEEDED(hr)) {
            source = scaler;
            width = scaled_width;
            height = scaled_height;
        }
    } else if (SUCCEEDED(hr)) {
        source = converted;
    }

    std::vector<std::uint8_t> pixels;
    if (SUCCEEDED(hr)) {
        pixels.resize(static_cast<size_t>(width) * height * 4);
        hr = source->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }

    if (SUCCEEDED(hr)) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(width);
        info.bmiHeader.biHeight = -static_cast<LONG>(height);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            hr = E_OUTOFMEMORY;
        } else {
            std::copy(pixels.begin(), pixels.end(), static_cast<std::uint8_t*>(bits));
            *bitmap = dib;
        }
    }

    safe_release(scaler);
    safe_release(converted);
    safe_release(frame);
    safe_release(decoder);
    safe_release(stream);
    safe_release(factory);
    return hr;
}

HRESULT set_reg_string(HKEY root, const std::wstring& subkey, const wchar_t* value_name, const std::wstring& value) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(result);
    }

    result = RegSetValueExW(key,
                            value_name,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(result);
}

std::wstring module_path() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(g_module, path, MAX_PATH);
    return path;
}

HRESULT register_thumbnail_provider(HKEY root) {
    const std::wstring clsid_key = std::wstring(L"Software\\Classes\\CLSID\\") + kClsidString;
    const std::wstring inproc_key = clsid_key + L"\\InprocServer32";
    const std::wstring extension_key = L"Software\\Classes\\.dom3d";
    const std::wstring progid_key = std::wstring(L"Software\\Classes\\") + kProgId;
    const std::wstring extension_thumbnail_key = extension_key + L"\\shellex\\" + kThumbnailHandlerClsid;
    const std::wstring progid_thumbnail_key = progid_key + L"\\shellex\\" + kThumbnailHandlerClsid;

    HRESULT hr = set_reg_string(root, clsid_key, nullptr, L"Dom3D Thumbnail Provider");
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, inproc_key, nullptr, module_path());
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, inproc_key, L"ThreadingModel", L"Apartment");
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, extension_key, nullptr, kProgId);
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, extension_key, L"Content Type", L"application/x-dom3d");
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, progid_key, nullptr, L"Dom3D Project");
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, extension_thumbnail_key, nullptr, kClsidString);
    }
    if (SUCCEEDED(hr)) {
        hr = set_reg_string(root, progid_thumbnail_key, nullptr, kClsidString);
    }
    return hr;
}

} // namespace

class Dom3DThumbnailProvider final : public IThumbnailProvider, public IInitializeWithStream {
public:
    Dom3DThumbnailProvider() {
        InterlockedIncrement(&g_dll_refs);
    }

    ~Dom3DThumbnailProvider() {
        safe_release(stream_);
        InterlockedDecrement(&g_dll_refs);
    }

    IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IThumbnailProvider) {
            *object = static_cast<IThumbnailProvider*>(this);
        } else if (iid == IID_IInitializeWithStream) {
            *object = static_cast<IInitializeWithStream*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&refs_);
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) {
            delete this;
        }
        return refs;
    }

    IFACEMETHODIMP Initialize(IStream* stream, DWORD) override {
        if (stream_) {
            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        }
        if (!stream) {
            return E_INVALIDARG;
        }
        stream_ = stream;
        stream_->AddRef();
        return S_OK;
    }

    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* bitmap, WTS_ALPHATYPE* alpha_type) override {
        if (!bitmap || !alpha_type || !stream_) {
            return E_INVALIDARG;
        }

        *bitmap = nullptr;
        *alpha_type = WTSAT_ARGB;

        LARGE_INTEGER origin{};
        stream_->Seek(origin, STREAM_SEEK_SET, nullptr);
        const std::vector<std::uint8_t> file_data = read_stream(stream_);
        const std::string base64 = extract_thumbnail_base64(file_data);
        const std::vector<std::uint8_t> png_data = decode_base64(base64);
        if (png_data.empty()) {
            return E_FAIL;
        }

        return create_hbitmap_from_png(png_data, cx, bitmap);
    }

private:
    long refs_ = 1;
    IStream* stream_ = nullptr;
};

class Dom3DClassFactory final : public IClassFactory {
public:
    Dom3DClassFactory() {
        InterlockedIncrement(&g_dll_refs);
    }

    ~Dom3DClassFactory() {
        InterlockedDecrement(&g_dll_refs);
    }

    IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IClassFactory) {
            *object = static_cast<IClassFactory*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&refs_);
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) {
            delete this;
        }
        return refs;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** object) override {
        if (outer) {
            return CLASS_E_NOAGGREGATION;
        }

        auto* provider = new (std::nothrow) Dom3DThumbnailProvider();
        if (!provider) {
            return E_OUTOFMEMORY;
        }

        const HRESULT hr = provider->QueryInterface(iid, object);
        provider->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) {
            InterlockedIncrement(&g_dll_refs);
        } else {
            InterlockedDecrement(&g_dll_refs);
        }
        return S_OK;
    }

private:
    long refs_ = 1;
};

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (clsid != kDom3DThumbnailProviderClsid) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) Dom3DClassFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }

    const HRESULT hr = factory->QueryInterface(iid, object);
    factory->Release();
    return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return g_dll_refs == 0 ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllRegisterServer() {
    HRESULT hr = register_thumbnail_provider(HKEY_LOCAL_MACHINE);
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
        hr = register_thumbnail_provider(HKEY_CURRENT_USER);
    }

    if (SUCCEEDED(hr)) {
        set_reg_string(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                       kClsidString,
                       L"Dom3D Thumbnail Provider");
        set_reg_string(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                       kClsidString,
                       L"Dom3D Thumbnail Provider");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return hr;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    RegDeleteTreeW(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\CLSID\\") + kClsidString).c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.dom3d\\shellex");
    RegDeleteTreeW(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + kProgId + L"\\shellex").c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (std::wstring(L"Software\\Classes\\CLSID\\") + kClsidString).c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.dom3d\\shellex");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (std::wstring(L"Software\\Classes\\") + kProgId + L"\\shellex").c_str());
    RegDeleteKeyValueW(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                       kClsidString);
    RegDeleteKeyValueW(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                       kClsidString);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
