#include <napi.h>
#include <memory>
#include "SharedMemory.hpp" 

class SharedMemoryWrapper : public Napi::ObjectWrap<SharedMemoryWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "SharedMemory", {
            InstanceMethod("getSize", &SharedMemoryWrapper::GetSize),
            InstanceMethod("getName", &SharedMemoryWrapper::GetName),
            InstanceMethod("lock", &SharedMemoryWrapper::Lock),
            InstanceMethod("unlock", &SharedMemoryWrapper::Unlock),
            InstanceMethod("getBuffer", &SharedMemoryWrapper::GetBuffer)
        });

        Napi::FunctionReference* constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(func);
        env.SetInstanceData(constructor);

        exports.Set("SharedMemory", func);
        return exports;
    }

    SharedMemoryWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<SharedMemoryWrapper>(info) {
        Napi::Env env = info.Env();

        if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
            Napi::TypeError::New(env, "Expected (name: string, size: number)").ThrowAsJavaScriptException();
            return;
        }

        std::string name = info[0].As<Napi::String>().Utf8Value();
        size_t size = info[1].As<Napi::Number>().Uint32Value();

        try {
            m_shm = std::make_shared<Shin::System::SharedMemory>(name, size);
        } catch (const std::exception& e) {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        }
    }

private:
    std::shared_ptr<Shin::System::SharedMemory> m_shm;

    Napi::Value GetSize(const Napi::CallbackInfo& info) {
        return Napi::Number::New(info.Env(), m_shm->GetSize());
    }

    Napi::Value GetName(const Napi::CallbackInfo& info) {
        return Napi::String::New(info.Env(), m_shm->GetName());
    }

    Napi::Value Lock(const Napi::CallbackInfo& info) {
        int timeoutMs = -1;
        if (info.Length() > 0 && info[0].IsNumber()) {
            timeoutMs = info[0].As<Napi::Number>().Int32Value();
        }
        bool success = m_shm->Lock(timeoutMs);
        return Napi::Boolean::New(info.Env(), success);
    }

    Napi::Value Unlock(const Napi::CallbackInfo& info) {
        m_shm->Unlock();
        return info.Env().Undefined();
    }

    Napi::Value GetBuffer(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        void* pBuf = m_shm->GetBuffer();
        size_t size = m_shm->GetSize();

        if (!pBuf) {
            return env.Null();
        }

        auto* keepAlive = new std::shared_ptr<Shin::System::SharedMemory>(m_shm);

        return Napi::Buffer<char>::New(env, static_cast<char*>(pBuf), size,
            [](Napi::Env /*env*/, char* /*data*/, std::shared_ptr<Shin::System::SharedMemory>* hint) {
                delete hint;
            }, keepAlive);
    }
};

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    return SharedMemoryWrapper::Init(env, exports);
}

NODE_API_MODULE(shm_addon, InitAll)