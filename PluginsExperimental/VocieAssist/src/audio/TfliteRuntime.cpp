#include "audio/TfliteRuntime.h"

#include <dlfcn.h>

#include <cstring>
#include <string>
#include <utility>

#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag = "tflite";

// Mirror of the C API's TfLiteQuantizationParams struct.
struct TfLiteQuantizationParamsC {
    float    scale;
    int32_t  zero_point;
};

struct TfliteSyms {
    void*    (*ModelCreateFromFile)(const char* model_path)               = nullptr;
    void     (*ModelDelete)(void* model)                                  = nullptr;
    void*    (*InterpreterOptionsCreate)()                                = nullptr;
    void     (*InterpreterOptionsDelete)(void* opts)                      = nullptr;
    void*    (*InterpreterCreate)(const void* model, const void* opts)    = nullptr;
    void     (*InterpreterDelete)(void* interp)                           = nullptr;
    int      (*InterpreterAllocateTensors)(void* interp)                  = nullptr;
    int      (*InterpreterResizeInputTensor)(void* interp, int32_t input_index, const int32_t* input_dims, int32_t input_dims_size) = nullptr;
    int      (*InterpreterInvoke)(void* interp)                           = nullptr;
    int32_t  (*InterpreterGetInputTensorCount)(const void* interp)        = nullptr;
    int32_t  (*InterpreterGetOutputTensorCount)(const void* interp)       = nullptr;
    void*    (*InterpreterGetInputTensor)(const void* interp, int32_t i)  = nullptr;
    const void* (*InterpreterGetOutputTensor)(const void* interp, int32_t i) = nullptr;
    int      (*TensorType)(const void* tensor)                            = nullptr;
    int32_t  (*TensorNumDims)(const void* tensor)                         = nullptr;
    int32_t  (*TensorDim)(const void* tensor, int32_t dim_index)          = nullptr;
    size_t   (*TensorByteSize)(const void* tensor)                        = nullptr;
    TfLiteQuantizationParamsC (*TensorQuantizationParams)(const void* t)  = nullptr;
    int      (*TensorCopyFromBuffer)(void* tensor, const void* data, size_t bytes) = nullptr;
    int      (*TensorCopyToBuffer)(const void* tensor, void* data, size_t bytes)   = nullptr;
};

struct LibraryState {
    void* handle = nullptr;
    TfliteSyms syms{};
    bool resolved = false;
    std::string error;
};

LibraryState& GetLibraryState() {
    static LibraryState state;
    return state;
}

#define LVA_DLSYM(state, FIELD, NAME)                                          \
    do {                                                                       \
        void* sym = dlsym((state).handle, NAME);                               \
        if (sym == nullptr) {                                                  \
            (state).error =                                                    \
                std::string("dlsym failed for ") + NAME + ": " +               \
                (dlerror() ? dlerror() : "unknown");                           \
            return false;                                                      \
        }                                                                      \
        (state).syms.FIELD =                                                   \
            reinterpret_cast<decltype((state).syms.FIELD)>(sym);                \
    } while (0)

bool LoadLibrary(LibraryState& state) {
    if (state.resolved) return true;

    std::string first_err;
    for (const auto& path : TfliteRuntime::DefaultLibrarySearchPaths()) {
        dlerror();
        void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (h != nullptr) {
            state.handle = h;
            LVA_LOGI(kTag, "loaded %s", path.c_str());
            break;
        }
        const char* err_raw = dlerror();
        const std::string err = err_raw ? err_raw : "(no error reported)";
        LVA_LOGD(kTag, "dlopen(%s) failed: %s", path.c_str(), err.c_str());
        if (first_err.empty()) {
            first_err = path + ": " + err;
        }
    }
    if (state.handle == nullptr) {
        state.error = first_err.empty()
            ? "no libtensorflowlite_c.so found in any search path"
            : "all dlopen attempts failed; first error: " + first_err;
        return false;
    }

    LVA_DLSYM(state, ModelCreateFromFile,         "TfLiteModelCreateFromFile");
    LVA_DLSYM(state, ModelDelete,                 "TfLiteModelDelete");
    LVA_DLSYM(state, InterpreterOptionsCreate,    "TfLiteInterpreterOptionsCreate");
    LVA_DLSYM(state, InterpreterOptionsDelete,    "TfLiteInterpreterOptionsDelete");
    LVA_DLSYM(state, InterpreterCreate,           "TfLiteInterpreterCreate");
    LVA_DLSYM(state, InterpreterDelete,           "TfLiteInterpreterDelete");
    LVA_DLSYM(state, InterpreterAllocateTensors,  "TfLiteInterpreterAllocateTensors");
    LVA_DLSYM(state, InterpreterResizeInputTensor, "TfLiteInterpreterResizeInputTensor");
    LVA_DLSYM(state, InterpreterInvoke,           "TfLiteInterpreterInvoke");
    LVA_DLSYM(state, InterpreterGetInputTensorCount,
              "TfLiteInterpreterGetInputTensorCount");
    LVA_DLSYM(state, InterpreterGetOutputTensorCount,
              "TfLiteInterpreterGetOutputTensorCount");
    LVA_DLSYM(state, InterpreterGetInputTensor,   "TfLiteInterpreterGetInputTensor");
    LVA_DLSYM(state, InterpreterGetOutputTensor,  "TfLiteInterpreterGetOutputTensor");
    LVA_DLSYM(state, TensorType,                  "TfLiteTensorType");
    LVA_DLSYM(state, TensorNumDims,               "TfLiteTensorNumDims");
    LVA_DLSYM(state, TensorDim,                   "TfLiteTensorDim");
    LVA_DLSYM(state, TensorByteSize,              "TfLiteTensorByteSize");
    LVA_DLSYM(state, TensorQuantizationParams,    "TfLiteTensorQuantizationParams");
    LVA_DLSYM(state, TensorCopyFromBuffer,        "TfLiteTensorCopyFromBuffer");
    LVA_DLSYM(state, TensorCopyToBuffer,          "TfLiteTensorCopyToBuffer");

    state.resolved = true;
    return true;
}

#undef LVA_DLSYM

}  // namespace

const std::vector<std::string>& TfliteRuntime::DefaultLibrarySearchPaths() {
    static const std::vector<std::string> kPaths = {
        "/usr/lib/libtensorflowlite_c.so",
        "/usr/lib/python3.11/site-packages/pyopen_wakeword/lib/libtensorflowlite_c.so",
        "libtensorflowlite_c.so",  // ld.so search
    };
    return kPaths;
}

TfliteRuntime::TfliteRuntime() {
    lib_ok_ = EnsureLibraryLoaded();
}

TfliteRuntime::~TfliteRuntime() {
    interpreter_ = nullptr;
    model_ = nullptr;
}

bool TfliteRuntime::EnsureLibraryLoaded() {
    auto& state = GetLibraryState();
    if (!LoadLibrary(state)) {
        last_error_ = state.error;
        LVA_LOGE(kTag, "library load failed: %s", last_error_.c_str());
        return false;
    }
    return true;
}

void TfliteRuntime::Unload() {
    auto& s = GetLibraryState();
    if (interpreter_ != nullptr && s.syms.InterpreterDelete) {
        s.syms.InterpreterDelete(interpreter_);
    }
    interpreter_ = nullptr;
    if (model_ != nullptr && s.syms.ModelDelete) {
        s.syms.ModelDelete(model_);
    }
    model_ = nullptr;
    allocated_ = false;
}

bool TfliteRuntime::LoadModel(const std::string& model_path) {
    if (!lib_ok_) {
        last_error_ = "library not loaded";
        return false;
    }
    Unload();

    auto& s = GetLibraryState();
    model_ = s.syms.ModelCreateFromFile(model_path.c_str());
    if (model_ == nullptr) {
        last_error_ = "TfLiteModelCreateFromFile returned NULL for "
                    + model_path;
        return false;
    }

    interpreter_ = s.syms.InterpreterCreate(model_, nullptr);
    if (interpreter_ == nullptr) {
        s.syms.ModelDelete(model_);
        model_ = nullptr;
        last_error_ = "TfLiteInterpreterCreate returned NULL";
        return false;
    }

    LVA_LOGI(kTag, "loaded model: %s (inputs=%d outputs=%d)",
             model_path.c_str(),
             s.syms.InterpreterGetInputTensorCount(interpreter_),
             s.syms.InterpreterGetOutputTensorCount(interpreter_));
    last_error_.clear();
    return true;
}

int TfliteRuntime::InputCount() const noexcept {
    if (interpreter_ == nullptr) return 0;
    return GetLibraryState().syms.InterpreterGetInputTensorCount(interpreter_);
}

int TfliteRuntime::OutputCount() const noexcept {
    if (interpreter_ == nullptr) return 0;
    return GetLibraryState().syms.InterpreterGetOutputTensorCount(interpreter_);
}

namespace {

TfliteTensorInfo BuildTensorInfo(const void* tensor) {
    TfliteTensorInfo info;
    if (tensor == nullptr) return info;
    auto& s = GetLibraryState();

    info.type = static_cast<TfliteType>(s.syms.TensorType(tensor));

    const int dims = s.syms.TensorNumDims(tensor);
    info.shape.reserve(static_cast<std::size_t>(std::max(dims, 0)));
    for (int i = 0; i < dims; ++i) {
        info.shape.push_back(s.syms.TensorDim(tensor, i));
    }

    info.byte_size = s.syms.TensorByteSize(tensor);

    const TfLiteQuantizationParamsC qp = s.syms.TensorQuantizationParams(tensor);
    info.quant.scale      = qp.scale;
    info.quant.zero_point = qp.zero_point;
    return info;
}

}  // namespace

TfliteTensorInfo TfliteRuntime::InputInfo(int index) const {
    if (interpreter_ == nullptr) return {};
    void* t = GetLibraryState().syms.InterpreterGetInputTensor(interpreter_, index);
    return BuildTensorInfo(t);
}

TfliteTensorInfo TfliteRuntime::OutputInfo(int index) const {
    if (interpreter_ == nullptr) return {};
    const void* t = GetLibraryState().syms.InterpreterGetOutputTensor(interpreter_, index);
    return BuildTensorInfo(t);
}

bool TfliteRuntime::CopyInput(int index, const void* data, std::size_t bytes) {
    if (interpreter_ == nullptr) {
        last_error_ = "no model loaded";
        return false;
    }
    auto& s = GetLibraryState();
    void* t = s.syms.InterpreterGetInputTensor(interpreter_, index);
    if (t == nullptr) {
        last_error_ = "input tensor index out of range";
        return false;
    }
    if (s.syms.TensorCopyFromBuffer(t, data, bytes) != 0) {
        last_error_ = "TfLiteTensorCopyFromBuffer failed (size mismatch?)";
        return false;
    }
    return true;
}

bool TfliteRuntime::ResizeInput(int index, const std::vector<int>& shape) {
    if (interpreter_ == nullptr) { last_error_ = "no model loaded"; return false; }
    auto& s = GetLibraryState();
    std::vector<int32_t> dims(shape.begin(), shape.end());
    int rc = s.syms.InterpreterResizeInputTensor(
        interpreter_, static_cast<int32_t>(index),
        dims.data(), static_cast<int32_t>(dims.size()));
    if (rc != 0) { last_error_ = "ResizeInputTensor failed"; return false; }
    rc = s.syms.InterpreterAllocateTensors(interpreter_);
    if (rc != 0) { last_error_ = "AllocateTensors after resize failed"; return false; }
    allocated_ = true;
    return true;
}

bool TfliteRuntime::Invoke() {
    if (interpreter_ == nullptr) {
        last_error_ = "no model loaded";
        return false;
    }
    auto& s = GetLibraryState();
    if (!allocated_) {
        if (s.syms.InterpreterAllocateTensors(interpreter_) != 0) {
            last_error_ = "AllocateTensors failed on first Invoke";
            return false;
        }
        allocated_ = true;
    }
    if (s.syms.InterpreterInvoke(interpreter_) != 0) {
        last_error_ = "TfLiteInterpreterInvoke returned non-zero";
        return false;
    }
    return true;
}

bool TfliteRuntime::CopyOutput(int index, void* data, std::size_t bytes) {
    if (interpreter_ == nullptr) {
        last_error_ = "no model loaded";
        return false;
    }
    auto& s = GetLibraryState();
    const void* t = s.syms.InterpreterGetOutputTensor(interpreter_, index);
    if (t == nullptr) {
        last_error_ = "output tensor index out of range";
        return false;
    }
    if (s.syms.TensorCopyToBuffer(t, data, bytes) != 0) {
        last_error_ = "TfLiteTensorCopyToBuffer failed (size mismatch?)";
        return false;
    }
    return true;
}

}  // namespace lva::audio
