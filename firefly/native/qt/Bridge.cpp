#include "Bridge.h"
#include "EventLoop.h"
#include <LibCore/EventLoop.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibRequests/NetworkError.h>
#include <LibURL/Parser.h>
#include <UI/Qt/WebContentView.h>
#include <UI/Qt/StringUtils.h>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <filesystem>
#include <functional>

#include <unordered_map>
#include <unordered_set>

namespace Firefly {
namespace {
class Window final : public QWidget {
public:
    std::function<bool()> request_close;
    std::function<void()> resized;
    void closeEvent(QCloseEvent* event) override
    {
        // The JS facade handles cancellation and schedules actual destruction.
        if (request_close) request_close();
        event->ignore();
    }
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (resized) resized();
    }
};
JS::Value text(JS::VM& vm, StringView value)
{
    return JS::PrimitiveString::create(vm, Utf16String::from_utf8_with_replacement_character(value));
}
}
struct Bridge::Impl {
    struct State {
        Window* window;
        Ladybird::WebContentView* view;
        bool closing { false };
        bool painted { false };
        bool failed { false };
        bool pending { false };
        Optional<URL::URL> requested_url;
        std::unordered_set<u64> navigation_requests;
    };
    JS::VM& vm;
    JS::Realm& realm;
    Mite::EventLoop& jobs;
    Core::EventLoop& gui;
    std::optional<int>& exit_code;
    std::unique_ptr<QObject> context { std::make_unique<QObject>() };
    GC::Root<JS::FunctionObject> handler;
    GC::Root<JS::Object> api;
    std::unordered_map<int, State> windows;
    int next_id { 1 };
    bool stopping { false };

    Impl(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui, std::optional<int>& exit_code)
        : vm(vm), realm(realm), jobs(jobs), gui(gui), exit_code(exit_code) { }
    ~Impl() { stop(); }
    void stop()
    {
        if(stopping) return;
        stopping = true;
        while (!windows.empty()) delete windows.begin()->second.window;
        context.reset(); // Cancel queued Qt callbacks before the Mite VM goes away.
        handler = nullptr;
        api = nullptr;
    }
    void deliver(int id, ByteString const& type, ByteString const& data = {})
    {
        if (stopping || !handler) return;
        if (jobs.is_busy()) {
            QTimer::singleShot(1,context.get(),[this,id,type,data]{deliver(id,type,data);});
            return;
        }
        jobs.dispatch([&] { return JS::call(vm, *handler, JS::js_undefined(), JS::Value(id), text(vm,type), text(vm,data)); });
        if (jobs.status()) gui.quit(jobs.status());
    }
    void enqueue(int id, ByteString type, ByteString data = {})
    {
        if (stopping) return;
        QTimer::singleShot(0, context.get(), [this,id,type=move(type),data=move(data)] { deliver(id,type,data); });
    }
    JS::ThrowCompletionOr<State*> state(JS::VM& vm)
    {
        if (!vm.argument(0).is_int32()) return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
        auto found = windows.find(vm.argument(0).as_i32());
        if (found == windows.end() || found->second.closing)
            return vm.throw_completion<JS::Error>("BrowserWindow has been destroyed"_utf16);
        return &found->second;
    }
    void destroy(int id)
    {
        auto found = windows.find(id);
        if (found == windows.end() || found->second.closing) return;
        found->second.closing = true;
        found->second.window->hide();
        found->second.window->deleteLater();
    }
    int create(int width, int height, QString const& title, bool show)
    {
        auto id = next_id++;
        auto* window = new Window;
        window->setWindowTitle(title);
        window->resize(width,height);
        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(0,0,0,0);
        auto* view = new Ladybird::WebContentView(window);
        layout->addWidget(view);
        windows.emplace(id, State {window,view,false,false,false,false,{}, {}});
        QObject::connect(window, &QObject::destroyed, context.get(), [this,id] {
            windows.erase(id); enqueue(id,"closed");
        });
        window->request_close = [this,id] {enqueue(id,"close-request");return true;};
        window->resized = [this,id] {enqueue(id,"resize");};
        view->on_close = [this,id] {enqueue(id,"close-request");};
        view->on_load_start = [this,id] {enqueue(id,"load-start");};
        view->on_load_finish = [this,id](auto const& url) {
            auto found=windows.find(id);
            if(found==windows.end() || found->second.closing || found->second.failed || !found->second.requested_url.has_value()) return;
            auto& current=found->second;
            if(current.pending && !current.requested_url->equals(url,URL::ExcludeFragment::Yes)) return;
            current.pending=false;enqueue(id,"loaded",url.serialize().to_byte_string());
        };
        view->on_url_change = [this,id](auto const& url) {enqueue(id,"url",url.serialize().to_byte_string());};
        view->on_title_change = [this,id](auto const& title) {enqueue(id,"title",title.to_byte_string());};
        view->on_web_content_crashed = [this,id](auto) {enqueue(id,"crashed");};
        view->on_network_request_started = [this,id](u64 request, auto const& url, auto const&, auto const&, auto, auto, auto, bool navigation, auto) {
            auto found = windows.find(id);
            if (found != windows.end() && navigation && found->second.requested_url.has_value() && url.equals(*found->second.requested_url,URL::ExcludeFragment::Yes))
                found->second.navigation_requests.insert(request);
        };
        view->on_network_request_finished = [this,id](u64 request, auto, auto const&, auto const& error) {
            auto found = windows.find(id);
            if (found == windows.end() || !found->second.navigation_requests.erase(request)) return;
            if (error.has_value()) {
                found->second.failed=true;found->second.pending=false;
                enqueue(id,"failed",Requests::network_error_to_string(*error));
            }
        };
        QObject::connect(view, &Ladybird::WebContentView::ready_to_paint, context.get(), [this,id] {
            auto found=windows.find(id);
            if(found!=windows.end() && !found->second.painted) {found->second.painted=true;enqueue(id,"paint");}
        });
        if (show) window->show();
        return id;
    }
    GC::Ref<JS::Object> install()
    {
        auto host = JS::Object::create(realm,nullptr);
        api = host;
        host->define_native_function(realm,"setHandler"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            if(!vm.argument(0).is_function()) return vm.throw_completion<JS::TypeError>("Handler must be callable"_utf16);
            handler=vm.argument(0).as_function();return JS::js_undefined();
        },1,JS::default_attributes);
        host->define_native_function(realm,"create"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto title=TRY(vm.argument(2).to_utf16_string(vm));
            return JS::Value(create(TRY(vm.argument(0).to_i32(vm)),TRY(vm.argument(1).to_i32(vm)),qstring_from_utf16_string(title),vm.argument(3).to_boolean()));
        },4,JS::default_attributes);
        host->define_native_function(realm,"destroy"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            TRY(state(vm));destroy(vm.argument(0).as_i32());return JS::js_undefined();
        },1,JS::default_attributes);
        host->define_native_function(realm,"destroyNow"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            if(!vm.argument(0).is_int32()) return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
            auto found=windows.find(vm.argument(0).as_i32());
            if(found!=windows.end()) delete found->second.window;
            return JS::js_undefined();
        },1,JS::default_attributes);
        host->define_native_function(realm,"exit"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto code=TRY(vm.argument(0).to_i32(vm));
            exit_code=code;
            stop();
            gui.quit(code);
            return JS::js_undefined();
        },1,JS::default_attributes);
        host->define_native_function(realm,"load"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto* current=TRY(state(vm));
            auto input=TRY(vm.argument(1).to_utf16_string(vm));auto bytes=input.to_byte_string();
            auto url=URL::Parser::basic_parse(bytes);
            if(!url.has_value()) return vm.throw_completion<JS::TypeError>("Invalid URL"_utf16);
            if(url->scheme()=="file"sv) {
                auto path=url->file_path();std::error_code error;
                if(path.contains('\0') || !std::filesystem::is_regular_file(path.characters(),error))
                    return vm.throw_completion<JS::Error>("File does not exist or is not a regular file"_utf16);
            }
            current->requested_url=*url;current->navigation_requests.clear();current->failed=false;current->pending=true;
            current->view->load(*url);return JS::js_undefined();
        },2,JS::default_attributes);
        auto method=[&](Utf16View name,auto action) {
            host->define_native_function(realm,Utf16String::from_utf16(name),[this,action](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
                auto* current=TRY(state(vm));action(*current->window);return JS::js_undefined();
            },1,JS::default_attributes);
        };
        method("show"_utf16,[](auto& window){window.show();});
        method("hide"_utf16,[](auto& window){window.hide();});
        method("focus"_utf16,[](auto& window){window.raise();window.activateWindow();window.setFocus();});
        host->define_native_function(realm,"resize"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto* current=TRY(state(vm));current->window->resize(TRY(vm.argument(1).to_i32(vm)),TRY(vm.argument(2).to_i32(vm)));return JS::js_undefined();
        },3,JS::default_attributes);
        host->define_native_function(realm,"size"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto* current=TRY(state(vm));JS::Value values[]{JS::Value(current->window->width()),JS::Value(current->window->height())};
            return JS::Array::create_from(realm,values);
        },1,JS::default_attributes);
        host->define_native_function(realm,"visible"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {return JS::Value(TRY(state(vm))->window->isVisible());},1,JS::default_attributes);
        host->define_native_function(realm,"pid"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {return JS::Value(static_cast<double>(TRY(state(vm))->view->web_content_pid()));},1,JS::default_attributes);
        host->define_native_function(realm,"title"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            auto* current=TRY(state(vm));current->window->setWindowTitle(qstring_from_utf16_string(TRY(vm.argument(1).to_utf16_string(vm))));return JS::js_undefined();
        },2,JS::default_attributes);
        host->define_native_function(realm,"titleText"_utf16,[this](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value> {
            return JS::PrimitiveString::create(vm,utf16_string_from_qstring(TRY(state(vm))->window->windowTitle()));
        },1,JS::default_attributes);
        host->define_native_function(realm,"quit"_utf16,[this](JS::VM&)->JS::ThrowCompletionOr<JS::Value> {gui.quit(0);return JS::js_undefined();},0,JS::default_attributes);
        return host;
    }
};
Bridge::Bridge(JS::VM& vm,JS::Realm& realm,Mite::EventLoop& jobs,Core::EventLoop& gui,std::optional<int>& exit_code)
    :m_impl(std::make_unique<Impl>(vm,realm,jobs,gui,exit_code)) { }
Bridge::~Bridge()=default;
GC::Ref<JS::Object> Bridge::exports() {return m_impl->install();}
void Bridge::ready() {m_impl->enqueue(0,"ready");}
}
