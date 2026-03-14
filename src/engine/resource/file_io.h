#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#endif

namespace eb {

// Cross-platform file reading abstraction.
// On desktop: reads from the filesystem.
// On Android: reads from AAssetManager (APK bundled assets).
class FileIO {
public:
    // Read an entire file into a byte buffer. Returns empty on failure.
    static std::vector<uint8_t> read_file(const std::string& path);

    // Read an entire file as a char vector (for shader loading).
    static std::vector<char> read_file_chars(const std::string& path);

#ifdef __ANDROID__
    // Must be called once at startup on Android before any file reads.
    static void set_asset_manager(AAssetManager* mgr);
    static AAssetManager* asset_manager();
private:
    static AAssetManager* s_asset_manager;
#endif
};

} // namespace eb
