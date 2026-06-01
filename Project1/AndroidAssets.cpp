#ifdef ANDROID
// tiny_gltf.h를 구현부 없이 포함 (TINYGLTF_IMPLEMENTATION 미정의)
// 구현부는 Model.cpp 한 곳에서만 컴파일된다.
#include "External/tinygltf/tiny_gltf.h"
#include "AndroidAssets.h"
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)

namespace jhb {

static AAssetManager* g_assetManager = nullptr;

void setAndroidAssetManager(AAssetManager* mgr)
{
    g_assetManager = mgr;
}

AAssetManager* getAndroidAssetManager()
{
    return g_assetManager;
}

// -------------------------------------------------------
// tinygltf FsCallbacks — APK assets를 AAssetManager로 읽음
// -------------------------------------------------------

static bool androidFileExists(const std::string& path, void* userdata)
{
    AAssetManager* mgr = static_cast<AAssetManager*>(userdata);
    AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) {
        AAsset_close(asset);
        return true;
    }
    return false;
}

// 경로를 그대로 반환 (assets 루트 기준 상대 경로 유지)
static std::string androidExpandFilePath(const std::string& path, void*)
{
    return path;
}

static bool androidReadWholeFile(std::vector<unsigned char>* out, std::string* err,
                                  const std::string& path, void* userdata)
{
    AAssetManager* mgr = static_cast<AAssetManager*>(userdata);
    AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        if (err) *err = "AAssetManager: failed to open: " + path;
        LOGE("androidReadWholeFile: cannot open '%s'", path.c_str());
        return false;
    }
    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    out->resize(size);
    AAsset_read(asset, out->data(), size);
    AAsset_close(asset);
    return true;
}

static bool androidWriteWholeFile(std::string* err, const std::string& path,
                                   const std::vector<unsigned char>&, void*)
{
    if (err) *err = "Write not supported for Android APK assets";
    return false;
}

static bool androidGetFileSizeInBytes(size_t* size_out, std::string* err,
                                       const std::string& path, void* userdata)
{
    AAssetManager* mgr = static_cast<AAssetManager*>(userdata);
    AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_UNKNOWN);
    if (!asset) {
        if (err) *err = "AAssetManager: failed to open: " + path;
        return false;
    }
    *size_out = static_cast<size_t>(AAsset_getLength(asset));
    AAsset_close(asset);
    return true;
}

void setupGltfAndroidCallbacks(tinygltf::TinyGLTF& context)
{
    tinygltf::FsCallbacks callbacks{};
    callbacks.FileExists        = androidFileExists;
    callbacks.ExpandFilePath    = androidExpandFilePath;
    callbacks.ReadWholeFile     = androidReadWholeFile;
    callbacks.WriteWholeFile    = androidWriteWholeFile;
    callbacks.GetFileSizeInBytes = androidGetFileSizeInBytes;
    callbacks.user_data         = g_assetManager;
    context.SetFsCallbacks(callbacks);
}

} // namespace jhb
#endif // ANDROID
