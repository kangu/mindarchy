#include "windowsdialogs.h"
#include "engine.h"
#include <QQuickWindow>
#include <QFileInfo>
#include <QTextDocument>
#include <QTimer>
#include <QDir>
#include <windows.h>
#include <shobjidl.h>
#include <commctrl.h>

namespace {
QString documentTitle(Engine *document) {
    if (!document->documentPath().isEmpty()) return QFileInfo(document->documentPath()).fileName();
    QTextDocument text;
    text.setHtml(document->nodes().value(1).text);
    const auto title = text.toPlainText().simplified().left(100);
    return title.isEmpty() ? QStringLiteral("New mindmap") : title;
}
void invoke(QQuickWindow *window, const char *method) {
    QTimer::singleShot(0, window, [window, method] { QMetaObject::invokeMethod(window, method); });
}
}

void installWindowsDialogs(Engine *document, QQuickWindow *window) {
    QObject::connect(document, &Engine::nativeCloseRequested, window, [document, window] {
        const auto title = QStringLiteral("Save changes to “%1”?").arg(documentTitle(document)).toStdWString();
        const TASKDIALOG_BUTTON buttons[] = {{100, L"Save"}, {101, L"Don't Save"}, {IDCANCEL, L"Cancel"}};
        TASKDIALOGCONFIG config = {};
        config.cbSize = sizeof(config);
        config.hwndParent = reinterpret_cast<HWND>(window->winId());
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        config.pszWindowTitle = L"Mindarchy";
        config.pszMainInstruction = title.c_str();
        config.pszContent = L"Your changes will be lost if you don't save them.";
        config.cButtons = 3;
        config.pButtons = buttons;
        config.nDefaultButton = 100;
        int choice = IDCANCEL;
        const auto result = TaskDialogIndirect(&config, &choice, nullptr, nullptr);
        if (FAILED(result)) choice = IDCANCEL;
        invoke(window, choice == 100 ? "saveBeforeClosing" : choice == 101 ? "approveClose" : "cancelClose");
    });
    QObject::connect(document, &Engine::nativeSaveRequested, window, [document, window] {
        QString path;
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IFileSaveDialog *dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            const COMDLG_FILTERSPEC types[] = {{L"Mindarchy document (*.omm)", L"*.omm"}};
            dialog->SetFileTypes(1, types);
            dialog->SetDefaultExtension(L"omm");
            DWORD options = 0;
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_STRICTFILETYPES);
            auto name = documentTitle(document);
            for (QChar c : QStringLiteral("<>:\"/\\|?*")) name.replace(c, '-');
            const auto wideName = name.toStdWString();
            dialog->SetFileName(wideName.c_str());
            if (SUCCEEDED(dialog->Show(reinterpret_cast<HWND>(window->winId())))) {
                IShellItem *item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item))) {
                    PWSTR filename = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filename))) {
                        path = QDir::fromNativeSeparators(QString::fromWCharArray(filename));
                        CoTaskMemFree(filename);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
        QTimer::singleShot(0, window, [window, path] {
            QMetaObject::invokeMethod(window, "finishSaveDialog", Q_ARG(QVariant, QVariant(path)));
        });
    });
}
