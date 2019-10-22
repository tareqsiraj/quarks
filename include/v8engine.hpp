// v8engine.cpp : Defines the entry point for the console application.
//

#ifndef _V8ENGINE_H_
#define _V8ENGINE_H_

#define _V8_LATEST

#include "v8.h"

#ifdef _V8_LATEST
#include "libplatform/libplatform.h"
#endif

#include <string>



// Create a stack-allocated handle scope.
#define V8_ENGINE(x) \
    v8::HandleScope handle_scope; \
    v8::Persistent<v8::Context> context = v8::Context::New(); \
    v8::Context::Scope context_scope(context); \
    v8Engine x(context)

#ifdef _V8_LATEST

#define v8EngineInitializeInMain(argc, argv) \
    v8::V8::InitializeICUDefaultLocation(argv[0]); \
    v8::V8::InitializeExternalStartupData(argv[0]); \
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform(); \
    v8::V8::InitializePlatform(platform.get()); \
    v8::V8::Initialize()


#define v8EngineShutdownInMain() \
    v8::V8::Dispose(); \
    v8::V8::ShutdownPlatform() \


struct v8Engine{
    v8Engine(std::function<void (std::string)> logFunc = nullptr);
     ~v8Engine();
    
    
    struct v8Context{
        v8Context(v8::Isolate*& iso, v8::Local<v8::Context>& ctx) : isolate(iso), context(ctx){
            
        }
        
        v8::Isolate*& isolate;
        v8::Local<v8::Context>& context;
    };
    
    void run(std::function<void (v8Context&)> onReady);
    
    std::string invoke(v8Context& context, std::string funcName, std::string elem, std::string args);
    
    v8::Local<v8::String> _ReadFile(const char* name);
    
    void setResult(bool result){
        _result = result;
    }
    
    bool getResult(){
        return _result;
    }
    
    void log(std::string s){
        if(_logFunc){
            _logFunc(s.c_str());
        }
    }
    
private:
    inline std::string CallJSFunction(v8Context& context, std::string funcName,
                    v8::Handle<v8::Value> argList[], unsigned int argCount);
                   
    
    bool _result;
    
    
    //v8::Isolate* _isolate;
    //v8::Local<v8::Context> _context;

    std::function<void (std::string)> _logFunc;
};

#else

struct v8Engine{
    v8Engine(v8::Persistent<v8::Context>& ctx);
    ~v8Engine();
    
    v8::Handle<v8::Object> global;
    
    std::string invoke(const char* fncName, const char* element,
                        const char* arguments);
    
};

#endif

#endif // _V8ENGINE_H_