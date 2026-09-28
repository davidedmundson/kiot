
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lva::audio {

enum class TfliteType : int {
    kNoType  = 0,
    kFloat32 = 1,
    kInt32   = 2,
    kUInt8   = 3,
    kInt64   = 4,
    kInt16   = 7,
    kInt8    = 9,
};

struct TfliteQuantization {
    float scale       = 0.0f;  // 0 = not quantized
    int   zero_point  = 0;
};

struct TfliteTensorInfo {
    TfliteType         type = TfliteType::kNoType;
    std::vector<int>   shape;
    TfliteQuantization quant;
    std::size_t        byte_size = 0;
};

class TfliteRuntime {
   public:
    static const std::vector<std::string>& DefaultLibrarySearchPaths();

    TfliteRuntime();
    ~TfliteRuntime();

    TfliteRuntime(const TfliteRuntime&)            = delete;
    TfliteRuntime& operator=(const TfliteRuntime&) = delete;

    bool Ok() const noexcept { return lib_ok_; }

    const std::string& LastError() const noexcept { return last_error_; }

    // -------------- model + interpreter lifecycle --------------

    bool LoadModel(const std::string& model_path);

    // Free the current model + interpreter (no-op if nothing loaded).
    void Unload();

    // True between a successful LoadModel() and an Unload() / dtor.
    bool ModelLoaded() const noexcept { return interpreter_ != nullptr; }

    // -------------- tensor access --------------

    int InputCount()  const noexcept;
    int OutputCount() const noexcept;

    TfliteTensorInfo InputInfo(int index)  const;
    TfliteTensorInfo OutputInfo(int index) const;

    bool CopyInput(int index, const void* data, std::size_t bytes);

    bool ResizeInput(int index, const std::vector<int>& shape);

    bool Invoke();

    bool CopyOutput(int index, void* data, std::size_t bytes);

   private:
    bool EnsureLibraryLoaded();

    bool        lib_ok_       = false;
    bool        allocated_    = false;
    std::string last_error_;

    // Opaque, owned. nullptr when no model loaded.
    void*       model_        = nullptr;
    void*       interpreter_  = nullptr;
};

}  // namespace lva::audio
