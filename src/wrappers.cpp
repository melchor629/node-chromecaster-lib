#include <nan.h>
#include <vector>
#include <queue>
#include <string>
#include <cstring>
#include <algorithm>

#include "AudioInput.hpp"

#ifdef _MSC_VER
#define and &&
#define or ||
#define not !
#endif

namespace demo {

    using v8::FunctionTemplate;
    using v8::Function;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Value;
    using v8::Number;

    class AudioInputWrapper: public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);

            struct Message {
                const void* pcm;
                uint32_t size;
            };

        private:
            explicit AudioInputWrapper(const AudioInput::Options &opt);
            ~AudioInputWrapper();

            static void cbk(uint32_t size, const void* pcm, void* userData);
            static NAN_METHOD(New);
            static NAN_METHOD(open);
            static NAN_METHOD(pause);
            static NAN_METHOD(close);
            static NAN_METHOD(isOpen);
            static NAN_METHOD(isPaused);
            static NAN_METHOD(ErrorToString);
            static NAN_METHOD(GetDevices);
            static NAN_METHOD(loadPortaudioLibrary);
            static NAN_METHOD(isNativeLibraryLoaded);
            static void EmitMessage(uv_async_t *w);
            static void Destructor(void*);
            static Nan::Persistent<Function> constructor;
            static std::vector<AudioInputWrapper*> instances;

            AudioInput* ai;
            uv_async_t message_async;
            uv_mutex_t message_mutex;
            std::queue<Message*> message_queue;
            Nan::AsyncResource* asyncRes;
    };

    NAN_MODULE_INIT(init) {
        AudioInputWrapper::Init(target);
    }

    NODE_MODULE(AudioInput, init)


    AudioInputWrapper::AudioInputWrapper(const AudioInput::Options &opt) {
        ai = new AudioInput(opt);
        AudioInputWrapper::instances.push_back(this);
        uv_mutex_init(&message_mutex);
        asyncRes = new Nan::AsyncResource(Nan::New("AudioInputWrapper:emit").ToLocalChecked());
    }

    AudioInputWrapper::~AudioInputWrapper() {
        if(ai->isOpen())
            ai->close();
        uv_mutex_destroy(&message_mutex);
        delete[] ai->options.devName;
        delete ai;
        delete asyncRes;
        ai = nullptr;
        asyncRes = nullptr;

        //Delete this reference from `instances`
        auto pos = std::find(instances.begin(), instances.end(), this);
        instances.erase(pos);
    }

    NAN_MODULE_INIT(AudioInputWrapper::Init) {
        // Prepare constructor template
        Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("AudioInput").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Prototype
        Nan::SetPrototypeMethod(tpl, "open", open);
        Nan::SetPrototypeMethod(tpl, "close", close);
        Nan::SetPrototypeMethod(tpl, "pause", pause);
        Nan::SetPrototypeMethod(tpl, "isOpen", isOpen);
        Nan::SetPrototypeMethod(tpl, "isPaused", isPaused);
        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("AudioInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());

        auto audioInputErrorMethod = Nan::New<FunctionTemplate>(ErrorToString);
        Nan::Set(target, Nan::New("AudioInputError").ToLocalChecked(), Nan::GetFunction(audioInputErrorMethod).ToLocalChecked());
        auto getDevicesMethod = Nan::New<FunctionTemplate>(GetDevices);
        Nan::Set(target, Nan::New("GetDevices").ToLocalChecked(), Nan::GetFunction(getDevicesMethod).ToLocalChecked());
        auto loadPortaudioLibrary = Nan::New<FunctionTemplate>(AudioInputWrapper::loadPortaudioLibrary);
        Nan::Set(target, Nan::New("loadPortaudioLibrary").ToLocalChecked(), Nan::GetFunction(loadPortaudioLibrary).ToLocalChecked());
        auto isNativeLibraryLoaded = Nan::New<FunctionTemplate>(AudioInputWrapper::isNativeLibraryLoaded);
        Nan::Set(target, Nan::New("isNativeLibraryLoaded").ToLocalChecked(), Nan::GetFunction(isNativeLibraryLoaded).ToLocalChecked());

        AudioInput::staticInit();
        node::AtExit(AudioInputWrapper::Destructor, nullptr);
    }

    Nan::Persistent<Function> AudioInputWrapper::constructor;
    std::vector<AudioInputWrapper*> AudioInputWrapper::instances;
    NAN_METHOD(AudioInputWrapper::New) {
        if (info.IsConstructCall()) {
            // Invoked as constructor: `new AudioInputWrapper(...)`
            Local<Value> value2 = info[0];
            AudioInput::Options opt = {44100, 16, 2, 0, nullptr};
            if(!value2->IsUndefined() and value2->IsObject()) {
                Local<Object> value = value2->ToObject();
                auto sampleRate = Nan::Get(value, Nan::New("samplerate").ToLocalChecked());
                auto bps = Nan::Get(value, Nan::New("bps").ToLocalChecked());
                auto ch = Nan::Get(value, Nan::New("channels").ToLocalChecked());
                auto devName = Nan::Get(value, Nan::New("deviceName").ToLocalChecked());
                auto timeFrame = Nan::Get(value, Nan::New("timePerFrame").ToLocalChecked());

                if(!sampleRate.IsEmpty()) {
                    Local<Value> v;
                    if(sampleRate.ToLocal(&v)) {
                        opt.sampleRate = Nan::To<uint32_t>(v).FromMaybe(44100);
                        if(opt.sampleRate != 44100 && opt.sampleRate != 48000
                            && opt.sampleRate != 88200 && opt.sampleRate != 96000) {
                                opt.sampleRate = 44100;
                            }
                    }
                }

                if(!bps.IsEmpty()) {
                    Local<Value> v;
                    if(bps.ToLocal(&v)) {
                        opt.bitsPerSample = Nan::To<uint32_t>(v).FromMaybe(16);
                        if(opt.bitsPerSample != 16 && opt.bitsPerSample != 24 && opt.bitsPerSample != 32 && opt.bitsPerSample != 8) {
                            opt.bitsPerSample = 16;
                        }
                    }
                }

                if(!ch.IsEmpty()) {
                    Local<Value> v;
                    if(ch.ToLocal(&v) && v->IsNumber()) {
                        opt.channels = Nan::To<uint32_t>(v).FromMaybe(2);
                        if(opt.channels > 2 || opt.channels == 0) {
                            opt.channels = 2;
                        }
                    }
                }

                if(!devName.IsEmpty()) {
                    Local<Value> v;
                    if(devName.ToLocal(&v) && v->IsString()) {
                        Nan::Utf8String str(v);
                        opt.devName = new char[str.length() + 1];
                        strcpy((char*) opt.devName, *str);
                    }
                }

                if(!timeFrame.IsEmpty()) {
                    Local<Value> v;
                    if(timeFrame.ToLocal(&v) && v->IsNumber())
                        opt.frameDuration = Nan::To<uint32_t>(v).FromMaybe(0);
                }
            }

            AudioInputWrapper* obj = new AudioInputWrapper(opt);
            obj->message_async.data = obj;
            obj->Wrap(info.This());
            info.GetReturnValue().Set(info.This());
        } else {
            // Invoked as plain function `AudioInputWrapper(...)`, turn into construct call.
            const int argc = 1;
            Local<Value> argv[argc] = { info[0] };
            Local<Function> cons = Nan::New(constructor);
            info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
        }
    }

    void AudioInputWrapper::Destructor(void* nothing) {
        (void)nothing;

        for(auto it = AudioInputWrapper::instances.begin(); it != AudioInputWrapper::instances.end(); it++) {
            AudioInputWrapper* p = *it;
            if(p->ai->isOpen())
                p->ai->close();
            delete[] p->ai->options.devName;
            uv_mutex_destroy(&(p->message_mutex));
            delete p->ai;
            p->ai = nullptr;
        }

        AudioInput::staticDeinit();
    }

    void AudioInputWrapper::cbk(uint32_t size, const void* pcm, void* userData) {
        AudioInputWrapper* obj = (AudioInputWrapper*) userData;
        Message* m = new Message();
        m->pcm = pcm;
        m->size = size;
        uv_mutex_lock(&obj->message_mutex);
        obj->message_queue.push(m);
        uv_mutex_unlock(&obj->message_mutex);
        uv_async_send(&obj->message_async);
    }

    NAN_METHOD(AudioInputWrapper::open) {
        AudioInputWrapper* obj = Nan::ObjectWrap::Unwrap<AudioInputWrapper>(info.Holder());
        uv_async_init(uv_default_loop(), &obj->message_async, &AudioInputWrapper::EmitMessage);
        obj->ai->setInputCallback(AudioInputWrapper::cbk, obj);
        Local<Number> number = Nan::New(obj->ai->open());
        info.GetReturnValue().Set(number);
    }

    NAN_METHOD(AudioInputWrapper::isOpen) {
        AudioInputWrapper* obj = Nan::ObjectWrap::Unwrap<AudioInputWrapper>(info.Holder());
        info.GetReturnValue().Set(Nan::New(obj->ai->isOpen()));
    }

    NAN_METHOD(AudioInputWrapper::close) {
        AudioInputWrapper* obj = Nan::ObjectWrap::Unwrap<AudioInputWrapper>(info.Holder());
        obj->ai->close();
        uv_close((uv_handle_t*) &obj->message_async, nullptr);
        uv_unref((uv_handle_t*) &obj->message_async);
        info.GetReturnValue().Set(Nan::Undefined());
    }

    NAN_METHOD(AudioInputWrapper::pause) {
        AudioInputWrapper* obj = Nan::ObjectWrap::Unwrap<AudioInputWrapper>(info.Holder());
        obj->ai->pause();
        info.GetReturnValue().Set(Nan::Undefined());
    }

    NAN_METHOD(AudioInputWrapper::isPaused) {
        AudioInputWrapper* obj = Nan::ObjectWrap::Unwrap<AudioInputWrapper>(info.Holder());
        info.GetReturnValue().Set(Nan::New(obj->ai->isPaused()));
    }

    NAN_METHOD(AudioInputWrapper::loadPortaudioLibrary) {
        Local<Value> arg = info[0];
        if(arg->IsString()) {
            Nan::Utf8String argStr(arg);
            AudioInput::staticInit(*argStr);
            info.GetReturnValue().Set(Nan::True());
        } else {
            Nan::ThrowError("First argument must be a string");
            info.GetReturnValue().Set(Nan::Undefined());
        }
    }

    NAN_METHOD(AudioInputWrapper::isNativeLibraryLoaded) {
        info.GetReturnValue().Set(Nan::New(AudioInput::isLoaded()));
    }

    static void deleteUsingCpp(char* ptr, void*) {
        delete[] ptr;
    }

    void AudioInputWrapper::EmitMessage(uv_async_t *w) {
        AudioInputWrapper *input = static_cast<AudioInputWrapper*>(w->data);
        Nan::HandleScope scope;

        if(not input->ai->isOpen()) return;

        uv_mutex_lock(&input->message_mutex);
        while (!input->message_queue.empty()) {
            Message* message = input->message_queue.front();
            v8::Local<v8::Value> args[2];
            args[0] = Nan::New("data").ToLocalChecked();

            //Create a node.js Buffer for audio data
            args[1] = Nan::NewBuffer(
                (char*) message->pcm,
                message->size,
                deleteUsingCpp,
                nullptr
            ).ToLocalChecked();

            input->asyncRes->runInAsyncScope(input->handle(), "emit", 2, args);
            input->message_queue.pop();
            delete message;
        }
        uv_mutex_unlock(&input->message_mutex);
    }

    NAN_METHOD(AudioInputWrapper::ErrorToString) {
        int errorCode = Nan::To<int32_t>(info[0]).FromJust();
        const char* str = AudioInput::errorCodeToString(errorCode);
        if(str != nullptr) {
            info.GetReturnValue().Set(Nan::New(str).ToLocalChecked());
        } else {
            info.GetReturnValue().Set(Nan::Null());
        }
    }

    NAN_METHOD(AudioInputWrapper::GetDevices) {
        std::vector<std::string> list;
        Local<v8::Array> array = Nan::New<v8::Array>();

        AudioInput::getInputDevices(list);
        uint32_t pos = 0;
        for(auto it = list.begin(); it != list.end(); it++) {
            Nan::Set(array, pos++, Nan::New<v8::String>(*it).ToLocalChecked());
        }

        info.GetReturnValue().Set(array);
    }

}
