#include "engine/resource/file_io.h"

#include <cstdio>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FileIO", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FileIO", __VA_ARGS__)
#else
#include <fstream>
#endif

namespace eb {

#ifdef __ANDROID__

AAssetManager* FileIO::s_asset_manager = nullptr;

void FileIO::set_asset_manager(AAssetManager* mgr) {
    s_asset_manager = mgr;
    LOGI("Asset manager set");
}

AAssetManager* FileIO::asset_manager() {
    return s_asset_manager;
}

std::vector<uint8_t> FileIO::read_file(const std::string& path) {
    if (!s_asset_manager) {
        LOGE("AAssetManager not set! Call FileIO::set_asset_manager() first.");
        return {};
    }

    AAsset* asset = AAssetManager_open(s_asset_manager, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", path.c_str());
        return {};
    }

    off_t size = AAsset_getLength(asset);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    AAsset_read(asset, buffer.data(), static_cast<size_t>(size));
    AAsset_close(asset);

    LOGI("Loaded asset: %s (%ld bytes)", path.c_str(), static_cast<long>(size));
    return buffer;
}

std::vector<char> FileIO::read_file_chars(const std::string& path) {
    auto bytes = read_file(path);
    return std::vector<char>(bytes.begin(), bytes.end());
}

#else // Desktop

std::vector<uint8_t> FileIO::read_file(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "[FileIO] Failed to open: %s\n", path.c_str());
        return {};
    }

    auto size = file.tellg();
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

std::vector<char> FileIO::read_file_chars(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "[FileIO] Failed to open: %s\n", path.c_str());
        return {};
    }

    auto size = file.tellg();
    std::vector<char> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

#endif

} // namespace eb
