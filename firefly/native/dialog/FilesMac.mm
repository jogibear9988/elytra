/** Actual AppKit file panels, preserving mixed selection/package modes and native cancellation. */
#include "Operation.h"
#import <AppKit/AppKit.h>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QWidget>

@interface FireflyDialogFilterTarget : NSObject
@property (nonatomic, weak) NSSavePanel* panel;
@property (nonatomic, strong) NSArray<NSArray<NSString*>*>* groups;
- (void)selectFilter:(NSPopUpButton*)sender;
@end
@implementation FireflyDialogFilterTarget

- (void)selectFilter:(NSPopUpButton*)sender
{
    auto index = sender.indexOfSelectedItem;
    if (index < 0 || static_cast<NSUInteger>(index) >= self.groups.count)
        return;
    auto values = self.groups[index];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // AppKit supports arbitrary extension strings through this API. Match the
    // native Electron chooser rather than drop unknown application file types.
    self.panel.allowedFileTypes = [values containsObject:@"*"] ? nil : values;
#pragma clang diagnostic pop
}

@end

namespace Firefly::Dialogs::Detail {
namespace {
    NSString* native(QString const& value)
    {
        return [[NSString alloc] initWithCharacters:reinterpret_cast<unichar const*>(value.utf16())
                                             length:static_cast<NSUInteger>(value.size())];
    }

    QString text(NSString* value)
    {
        if (!value)
            return {};
        auto bytes = [value dataUsingEncoding:NSUTF8StringEncoding];
        return bytes ? QString::fromUtf8(static_cast<char const*>(bytes.bytes),
                           static_cast<qsizetype>(bytes.length))
                     : QString {};
    }

    Result failure(char const* code, char const* message)
    {
        return { {}, code, message };
    }

    struct FileState {
        NSSavePanel* panel {};
        FireflyDialogFilterTarget* filters {};
        Completion complete;
        bool open {}, finished {}, cancel_requested {};
    };

    Result selection(std::shared_ptr<FileState> const& state, NSModalResponse response)
    {
        if (response != NSModalResponseOK || state->cancel_requested) {
            return { state->open
                    ? QJsonObject { { "canceled", true }, { "filePaths", QJsonArray {} } }
                    : QJsonObject { { "canceled", true }, { "filePath", QString {} } },
                {}, {} };
        }

        NSOpenPanel* open = (NSOpenPanel*)state->panel;
        NSArray<NSURL*>* urls
            = state->open ? open.URLs : (state->panel.URL ? @[ state->panel.URL ] : @[]);
        if (!urls.count || urls.count > 256)
            return failure(
                "ERR_FIREFLY_DIALOG_DATA", "Native dialog returned an invalid path count");
        QJsonArray paths;
        qsizetype total {};
        for (NSURL* url in urls) {
            auto path = text(url.path);
            auto bytes = path.toUtf8();
            if (!url.isFileURL || path.isEmpty() || !QFileInfo(path).isAbsolute()
                || path.contains(QChar(0)) || bytes.size() > 32768
                || (total += bytes.size()) > 262144)
                return failure(
                    "ERR_FIREFLY_DIALOG_DATA", "Native dialog returned invalid or oversized paths");
            paths.append(path);
        }

        return { state->open ? QJsonObject { { "canceled", false }, { "filePaths", paths } }
                             : QJsonObject { { "canceled", false }, { "filePath", paths[0] } },
            {}, {} };
    }

    void finish(std::shared_ptr<FileState> const& state, NSModalResponse response)
    {
        if (state->finished)
            return;
        state->finished = true;
        Result result;
        @try {
            result = selection(state, response);
        } @catch (NSException*) {
            result
                = failure("ERR_FIREFLY_DIALOG_NATIVE", "Native file selection could not be read");
        }

        auto complete = std::move(state->complete);
        state->filters = nil;
        // Retain the native panel through its completion, then release our ownership.
        state->panel = nil;
        if (complete)
            complete(std::move(result));
    }

    class FileOperation final : public Operation {
    public:
        explicit FileOperation(std::shared_ptr<FileState> state)
            : state(std::move(state))
        {
        }

        ~FileOperation() override
        {
            cancel();
        }

        void cancel() override
        {
            if (state->finished || state->cancel_requested)
                return;
            state->cancel_requested = true;
            @try {
                auto parent = state->panel.sheetParent;
                if (parent)
                    [parent endSheet:state->panel returnCode:NSModalResponseCancel];
                else
                    [state->panel cancel:nil];
            } @catch (NSException*) {
                // Keep the operation charged until AppKit actually completes. A
                // cancellation request cannot pretend the system panel disappeared.
            }
        }

    private:
        std::shared_ptr<FileState> state;
    };

    void filters(std::shared_ptr<FileState> const& state, QJsonArray const& values)
    {
        if (values.isEmpty()) {
            state->panel.allowsOtherFileTypes = YES;
            return;
        }

        auto groups = [NSMutableArray array];
        auto names = [NSMutableArray array];
        for (auto value : values) {
            auto filter = value.toObject();
            auto extensions = [NSMutableOrderedSet orderedSet];
            for (auto item : filter.value("extensions").toArray()) {
                auto extension = item.toString();
                if (extension != "*")
                    extension = extension.section('.', -1);
                [extensions addObject:native(extension)];
            }

            [groups addObject:extensions.array];
            [names addObject:native(filter.value("name").toString())];
        }

        auto target = [FireflyDialogFilterTarget new];
        target.panel = state->panel;
        target.groups = groups;
        state->filters = target;
        auto popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(70, 0, 220, 28) pullsDown:NO];
        [popup addItemsWithTitles:names];
        popup.target = target;
        popup.action = @selector(selectFilter:);
        [target selectFilter:popup];
        if (groups.count > 1) {
            auto view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 30)];
            auto label = [NSTextField labelWithString:@"Format:"];
            label.frame = NSMakeRect(0, 4, 65, 20);
            [view addSubview:label];
            [view addSubview:popup];
            state->panel.accessoryView = view;
        }
    }

    void configure(std::shared_ptr<FileState> const& state, QJsonObject const& options)
    {
        auto panel = state->panel;
        if (!options.value("title").toString().isEmpty())
            panel.title = native(options.value("title").toString());
        if (!options.value("buttonLabel").toString().isEmpty())
            panel.prompt = native(options.value("buttonLabel").toString());
        if (!options.value("message").toString().isEmpty())
            panel.message = native(options.value("message").toString());
        if (!options.value("nameFieldLabel").toString().isEmpty())
            panel.nameFieldLabel = native(options.value("nameFieldLabel").toString());
        if (options.contains("showsTagField"))
            panel.showsTagField = options.value("showsTagField").toBool();
        filters(state, options.value("filters").toArray());
        panel.extensionHidden = NO;
        auto path = options.value("defaultPath").toString();
        if (!path.isEmpty()) {
            QFileInfo info(path);
            if (info.isDir())
                panel.directoryURL = [NSURL fileURLWithPath:native(info.absoluteFilePath())
                                                isDirectory:YES];
            else {
                // Resolve relative folder components against the admission cwd.
                panel.directoryURL = [NSURL fileURLWithPath:native(info.absolutePath())
                                                isDirectory:YES];
                panel.nameFieldStringValue = native(info.fileName());
            }
        }

        QStringList properties;
        for (auto item : options.value("properties").toArray())
            properties.append(item.toString());
        panel.showsHiddenFiles = properties.contains("showHiddenFiles");
        panel.treatsFilePackagesAsDirectories = properties.contains("treatPackageAsDirectory");
        if (properties.contains("createDirectory"))
            panel.canCreateDirectories = YES;
        if (state->open) {
            NSOpenPanel* open = (NSOpenPanel*)panel;
            open.canChooseFiles = properties.contains("openFile");
            open.canChooseDirectories = properties.contains("openDirectory");
            open.allowsMultipleSelection = properties.contains("multiSelections");
            open.resolvesAliases = !properties.contains("noResolveAliases");
        }
    }
}

Started start_files(
    QString const& kind, QJsonObject const& options, QWidget* parent, Completion complete)
{
    if (!NSThread.isMainThread)
        return { {},
            failure("ERR_FIREFLY_DIALOG_NATIVE", "File dialogs require the Cocoa owner thread") };
    // Snapshot cwd before AppKit factories can enter their native presentation path.
    auto normalized = options;
    auto requested = options.value("defaultPath").toString();
    if (!requested.isEmpty())
        normalized.insert("defaultPath", QFileInfo(requested).absoluteFilePath());
    auto state = std::make_shared<FileState>();
    state->open = kind == "open";
    state->complete = std::move(complete);
    std::shared_ptr<FileOperation> operation;
    @try {
        state->panel = state->open ? [NSOpenPanel openPanel] : [NSSavePanel savePanel];
        if (!state->panel)
            return { {}, failure("ERR_FIREFLY_DIALOG_NATIVE", "Native file panel unavailable") };
        configure(state, normalized);
        NSWindow* window = nil;
        if (parent) {
            NSView* view = (__bridge NSView*)reinterpret_cast<void*>(parent->winId());
            window = view.window;
            if (!window)
                return { {},
                    failure("ERR_FIREFLY_DIALOG_NATIVE", "Native parent window unavailable") };
        }

        operation = std::make_shared<FileOperation>(state);
        auto callback = ^(NSModalResponse response) { finish(state, response); };
        if (window)
            [state->panel beginSheetModalForWindow:window completionHandler:callback];
        else
            [state->panel beginWithCompletionHandler:callback];
        return { operation, {} };
    } @catch (NSException*) {
        return { operation,
            failure("ERR_FIREFLY_DIALOG_NATIVE", "Native file dialog could not be started") };
    }
}
}
