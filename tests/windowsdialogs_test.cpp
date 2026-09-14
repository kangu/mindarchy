#include "engine.h"
#include "windowsdialogs.h"
#include <QQuickWindow>
#include <QtTest>
#include <atomic>
#include <thread>
#include <windows.h>
#include <commctrl.h>

class DialogWindow : public QQuickWindow {
    Q_OBJECT
public:
    int decision = -1;
    QString savedPath;
    bool saveFinished = false;
public slots:
    void finishSaveDialog(QVariant path) { savedPath = path.toString(); saveFinished = true; }
    void saveBeforeClosing() { decision = 1; }
    void approveClose() { decision = 2; }
    void cancelClose() { decision = 0; }
};

class WindowsDialogsTest : public QObject {
    Q_OBJECT
private slots:
    void nativeSavePicker() {
        Engine document(nullptr, Engine::InitialContent::Blank);
        QVERIFY(document.setText(1, "Native save test"));
        DialogWindow window;
        window.resize(640, 480); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        installWindowsDialogs(&document, &window);
        QTemporaryDir folder;
        const auto path = QDir::toNativeSeparators(folder.filePath("Native save test.omm")).toStdWString();
        const auto owner = reinterpret_cast<HWND>(window.winId());
        std::atomic<bool> selected = false;
        std::thread responder([&] {
            const auto deadline = GetTickCount64() + 10000;
            while (GetTickCount64() < deadline) {
                HWND dialog = nullptr;
                while ((dialog = FindWindowExW(nullptr, dialog, L"#32770", nullptr))) {
                    if (GetWindow(dialog, GW_OWNER) != owner) continue;
                    HWND filename = nullptr;
                    EnumChildWindows(dialog, [](HWND child, LPARAM data) -> BOOL {
                        wchar_t cls[64] = {}, text[256] = {};
                        GetClassNameW(child, cls, 64); GetWindowTextW(child, text, 256);
                        if (wcscmp(cls, L"Edit") == 0 && wcsstr(text, L"Native save test")) {
                            *reinterpret_cast<HWND *>(data) = child; return FALSE;
                        }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&filename));
                    if (filename) {
                        SendMessageW(filename, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(path.c_str()));
                        PostMessageW(dialog, WM_COMMAND, IDOK, 0);
                        selected = true; return;
                    }
                }
                Sleep(25);
            }
        });
        emit document.nativeSaveRequested();
        responder.join();
        QVERIFY(selected);
        QTRY_VERIFY(window.saveFinished);
        QCOMPARE(window.savedPath, folder.filePath("Native save test.omm"));
        QVERIFY(document.save(window.savedPath));
        Engine reopened; QVERIFY(reopened.open(window.savedPath));
        QCOMPARE(reopened.selectedText(), QString("Native save test"));
    }
    void nativeCloseChoices_data() {
        QTest::addColumn<int>("button");
        QTest::addColumn<int>("expected");
        QTest::newRow("save") << 100 << 1;
        QTest::newRow("discard") << 101 << 2;
        QTest::newRow("cancel") << IDCANCEL << 0;
    }
    void nativeCloseChoices() {
        QFETCH(int, button);
        QFETCH(int, expected);
        Engine document(nullptr, Engine::InitialContent::Blank);
        DialogWindow window;
        window.setTitle("Mindarchy native dialog test");
        window.resize(640, 480);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        installWindowsDialogs(&document, &window);
        const auto owner = reinterpret_cast<HWND>(window.winId());
        std::atomic<bool> clicked = false;
        std::thread responder([&] {
            const auto deadline = GetTickCount64() + 10000;
            while (GetTickCount64() < deadline) {
                HWND dialog = nullptr;
                while ((dialog = FindWindowExW(nullptr, dialog, L"#32770", L"Mindarchy"))) {
                    if (GetWindow(dialog, GW_OWNER) == owner) {
                        clicked = true;
                        PostMessageW(dialog, TDM_CLICK_BUTTON, button, 0);
                        return;
                    }
                }
                Sleep(25);
            }
        });
        emit document.nativeCloseRequested();
        responder.join();
        QVERIFY2(clicked, "A real owned Windows TaskDialog must have appeared");
        QTRY_COMPARE(window.decision, expected);
    }
};
QTEST_MAIN(WindowsDialogsTest)
#include "windowsdialogs_test.moc"
