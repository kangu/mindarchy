#include <optional>
#include <functional>
#pragma once
#include "tabshortcuts.h"
#include <QHash>
#include <QObject>
#include <QRectF>
#include <QSet>
#include <QVariantList>
#include <QVector>
#include "theme.h"
#include "calendar.h"
#include "nodeimage.h"

struct MapNode {
    int id = 0, parent = -1;
    QVector<int> children;
    QString text, notes;
    QVariantList resources;
    NodeImage image;
    QString kind = "text";
    CalendarData calendar;
    QVariantMap meeting;
    QString meetingSection;
    QVariantMap style;
    bool folded = false, task = false, checked = false;
    QPointF manualOffset;
    QRectF rect;
    int depth = 0;
    int taskChildren = 0, completedTaskChildren = 0;
};
class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString textFamily READ textFamily NOTIFY changed)
    Q_PROPERTY(bool selectedHasImage READ selectedHasImage NOTIFY changed)
    Q_PROPERTY(QString selectedImagePlacement READ selectedImagePlacement NOTIFY changed)
    Q_PROPERTY(QVariantList tabShortcutHelp READ tabShortcutHelp CONSTANT)
    Q_PROPERTY(QString documentName READ documentName NOTIFY changed)
    Q_PROPERTY(bool edited READ edited NOTIFY changed)
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY changed)
    Q_PROPERTY(QString spacing READ spacing WRITE setSpacing NOTIFY changed)
    Q_PROPERTY(QStringList branchStyles READ branchStyles CONSTANT)
    Q_PROPERTY(QString branchStyle READ branchStyle WRITE setBranchStyle NOTIFY changed)
    Q_PROPERTY(bool manual READ manual WRITE setManual NOTIFY changed)
    Q_PROPERTY(int selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QVariantList selection READ selection NOTIFY changed)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY outlineChanged)
    Q_PROPERTY(QVariantMap selectedCalendar READ selectedCalendar NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedMeeting READ selectedMeeting NOTIFY changed)
    Q_PROPERTY(QString selectedEntryPrompt READ selectedEntryPrompt NOTIFY changed)
    Q_PROPERTY(QString selectedKind READ selectedKind NOTIFY changed)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY changed)
    Q_PROPERTY(QVariantList selectedResources READ selectedResources NOTIFY changed)
    Q_PROPERTY(QString selectedNotes READ selectedNotes NOTIFY changed)
    Q_PROPERTY(bool selectedTask READ selectedTask NOTIFY changed)
    Q_PROPERTY(bool selectedChecked READ selectedChecked NOTIFY changed)
    Q_PROPERTY(int selectedTaskChildren READ selectedTaskChildren NOTIFY changed)
    Q_PROPERTY(int selectedCompletedTasks READ selectedCompletedTasks NOTIFY changed)
    Q_PROPERTY(bool selectedFolded READ selectedFolded NOTIFY changed)
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY changed)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY changed)
    Q_PROPERTY(double layoutMs READ layoutMs NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY changed)
    Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(QVariantList nodeTemplates READ nodeTemplates CONSTANT)
    Q_PROPERTY(QVariantMap selectedStyle READ selectedStyle NOTIFY changed)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    Q_PROPERTY(QColor canvasColor READ canvasColor NOTIFY changed)
  public:
    QVariantList tabShortcutHelp() const { return TabShortcuts::help(); }
    enum class InitialContent { Example, Blank };
    explicit Engine(QObject *parent = nullptr, InitialContent content = InitialContent::Example);
    const QHash<int, MapNode> &nodes() const { return m_nodes; }
    const QVector<int> &visibleIds() const { return m_visible; }
    QRectF bounds() const { return m_bounds; }
    QHash<int,QRectF> manualGeometry(int movingId = -1, QPointF delta = {}) const;
    QHash<int,QRectF> manualGeometry(const QSet<int> &movingRoots, QPointF delta) const;
    QSet<int> selectedIds() const { return m_selection; }
    bool isDescendant(int node, int ancestor) const;
    QString layout() const { return m_layout; }
    QString spacing() const { return m_spacing; }
    QStringList branchStyles() const;
    QString branchStyle() const { return m_branchStyle; }
    bool manual() const { return m_manual; }
    int selectedId() const { return m_selected; }
    QVariantList selection() const;
    QVariantList outline() const;
    QString selectedKind() const { return m_nodes.value(m_selected).kind; }
    QVariantMap selectedCalendar() const;
    QVariantMap selectedMeeting() const { return m_nodes.value(m_selected).meeting; }
    QString selectedEntryPrompt() const;
    Q_INVOKABLE QVariantList searchNodes(QString query) const;
    bool revealSearchNode(int id);
    QVariantList nodeTemplates() const;
    Q_INVOKABLE QVariantMap templateCalendar(QString anchor = {}, int monthOffset = 0) const;
    Q_INVOKABLE bool addNodeTemplate(QString templateId, QString weekDate);
    Q_INVOKABLE bool updateMeeting(QString date, QString time, QString attendees);
    Q_INVOKABLE void addDateNode(QString view);
    Q_INVOKABLE bool setNodeKind(int id, QString kind);
    Q_INVOKABLE bool configureDateNode(int id, QString view, QString anchor);
    Q_INVOKABLE bool shiftDateNode(int id, int direction);
    Q_INVOKABLE bool setDateEntry(int id, QString date, QString text);
    Q_INVOKABLE QString dateEntry(int id, QString date) const;
    QString selectedText() const { return m_nodes.value(m_selected).text; }
    QVariantList selectedResources() const { return m_nodes.value(m_selected).resources; }
    Q_INVOKABLE bool importImage(int id, QString path);
    bool selectedHasImage() const { return hasImage(selectedId()); }
    QString selectedImagePlacement() const { return m_nodes.value(selectedId()).image.placement; }
    Q_INVOKABLE bool setImagePlacement(int id, QString placement);
    QSizeF previewContentSize(int id, const QString &text) const;
    bool setImage(int id, const NodeImage &image, bool preservePlacement = true);
    Q_INVOKABLE bool resizeImage(int id, double width);
    Q_INVOKABLE bool resetImageSize(int id) { return resizeImage(id,NodeImage::defaultWidth(m_nodes.value(id).image.pixels.size())); }
    Q_INVOKABLE bool removeImage(int id);
    Q_INVOKABLE QString imageSource(int id) const { return m_nodes.value(id).image.source(); }
    Q_INVOKABLE bool hasImage(int id) const { return !m_nodes.value(id).image.empty(); }
    Q_INVOKABLE void copyImage(int id);
    Q_INVOKABLE bool cutImage(int id);
    Q_INVOKABLE bool pasteImage(int id);
    Q_INVOKABLE bool clipboardHasImage() const;
    QSizeF contentSize(int id) const;
    Q_INVOKABLE bool setResource(int node, int index, QString kind, QString name, QString target);
    Q_INVOKABLE bool removeResource(int node, int index);
    Q_INVOKABLE bool openResource(int node, int index);
    QString selectedNotes() const { return m_nodes.value(m_selected).notes; }
    bool selectedTask() const { return m_nodes.value(m_selected).task; }
    int selectedTaskChildren() const { return m_nodes.value(m_selected).taskChildren; }
    int selectedCompletedTasks() const { return m_nodes.value(m_selected).completedTaskChildren; }
    bool selectedChecked() const { return m_nodes.value(m_selected).checked; }
    bool selectedFolded() const { return m_nodes.value(m_selected).folded; }
    int nodeCount() const { return m_nodes.size(); }
    int visibleCount() const { return m_visible.size(); }
    double layoutMs() const { return m_layoutMs; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString error() const { return m_error; }
    QVector<QPair<int, int>> connections() const { return m_connections; }
    int connectionCount() const { return m_connections.size(); }
    QString textFamily() const;
    QString themeId() const { return m_themeId; }
    QVariantList themes() const { return Themes::catalog(); }
    QColor canvasColor() const { return Themes::get(m_themeId).canvas; }
    NodeAppearance appearance(int id) const;
    QVariantMap selectedStyle() const;
    QStringList fontFamilies() const;
    Q_INVOKABLE bool applyNodeStyle(QVariantMap patch);
    Q_INVOKABLE void resetNodeStyle();
    Q_INVOKABLE void resetBranchWidth();
    Q_INVOKABLE void connectSelection();
    void setLayout(QString value);
    void setSpacing(QString value);
    void setBranchStyle(QString value);
    void setManual(bool value);
    void setThemeId(QString value);
    Q_INVOKABLE bool applyThemeRecipe(QString id);
    Q_INVOKABLE void select(int id, bool extend = false);
    void setWindowNavigation(std::function<QVariantList()> list, std::function<void(qint64)> activate) {
        m_listWindows=std::move(list); m_activateWindow=std::move(activate);
    }
    Q_INVOKABLE QVariantList applicationWindows() const;
    Q_INVOKABLE void activateApplicationWindow(qint64 pid);
    Q_INVOKABLE void cycleApplicationWindow(int direction);
    QByteArray branchData() const;
    bool pasteBranchData(const QByteArray &data);
    bool pasteOutline(const QString &text);
    Q_INVOKABLE bool copyBranches();
    Q_INVOKABLE bool pasteBranches();
    Q_INVOKABLE void addChild();
    void addChildFromPointer(int parent, std::optional<QPointF> position = {});
    Q_INVOKABLE void addSibling();
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void toggleFold();
    Q_INVOKABLE void toggleTask();
    Q_INVOKABLE void toggleChecked();
    Q_INVOKABLE bool setText(int id, QString text);
    QSizeF previewTextSize(int id, const QString &text) const;
    Q_INVOKABLE void selectMany(QVariantList ids, bool extend = false);
    Q_INVOKABLE void setNotes(QString text);
    Q_INVOKABLE void navigate(QString direction, bool extend = false);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void loadFixture(int count);
    Q_INVOKABLE void moveNode(int id, int parent, int beforeId = -1);
    QVector<int> branchRoots(const QSet<int> &selection) const;
    bool moveBranches(const QSet<int> &selection, int parent, int beforeId = -1, QPointF delta = {});
    Q_INVOKABLE void moveManual(int id, double dx, double dy);
    Q_INVOKABLE bool hasUnsavedChanges() const;
    QString documentName() const;
    bool edited() const;
    Q_INVOKABLE QString documentPath() const { return m_documentPath; }
    void setRecentDirectory(QString directory) { m_recentDirectory=std::move(directory); }
    Q_INVOKABLE QVariantList recentDocuments() const;
    Q_INVOKABLE QVariantList recentMaps() const;
    Q_INVOKABLE void clearRecentDocuments();
    Q_INVOKABLE bool requestOpenDocument(QString path);
    Q_INVOKABLE bool save(QString path);
    Q_INVOKABLE bool open(QString path);
    quint64 recoveryRevision() const { return m_documentRevision; }
    bool saveRecovery(const QString &path, const QVariantMap &ui);
    bool openRecovery(const QString &path, QVariantMap *ui = nullptr);
  signals:
    void clipboardMessage(QString text);
    void documentSaved();
    void documentOpening();
    void documentOpened();
    void openDocumentRequested(QString path);
    void nativeCloseRequested();
    void nativeSaveRequested();
    void nativeFolderMenuRequested(double x, double y);
    void quitRequested();
    void quitDecision(bool accepted);
    void windowCloseApproved(bool forget);
    void newDocumentRequested();
    void tabActionRequested(QString action, qint64 target);
    void changed();
    void outlineChanged();
    void editRequested(int id);

  private:
    std::function<QVariantList()> m_listWindows;
    std::function<void(qint64)> m_activateWindow;
    bool addMeetingTemplate();
    struct State {
        QHash<int, MapNode> nodes;
        QVector<QPair<int, int>> connections;
        QString layout, spacing, branchStyle, themeId;
        bool manual;
        int selected, nextId;
        QSet<int> selection;
    };
    bool loadDocumentBytes(const QByteArray &bytes, const QString &path);
    QByteArray documentBytes(QString destination = {}) const;
    QByteArray m_savedBytes;
    quint64 m_documentRevision = 0;
    mutable quint64 m_checkedRevision = ~quint64(0);
    mutable bool m_edited = false;
    QString m_documentPath;
    QString m_recentDirectory;
    State state() const;
    void restore(const State &state);
    void checkpoint();
    void rebuild();
    void add(int parent, int after = -1, QString kind = "text", QString dateView = "week", std::optional<QPointF> position = {}, bool emptyText = false);
    bool fail(const QString &error);
    // One entry per current node; font15, zero document margin and renderer padding
    // are fixed measurement constants. Text and task changes replace the entry.
    struct TextMeasure {
        QString text, plainText;
        bool task;
        QSizeF size;
        double width = 0;
        QString family;
        int depth = -1;
    };
    bool measureText(const QString &text, bool task, TextMeasure &result, double fixedWidth = 0, QString family = {}, int depth = 0) const;
    QHash<int, TextMeasure> m_textCache;
    QHash<int, int> m_branchIndices;
    QHash<int, QRectF> m_layoutRects;
    QHash<int, MapNode> m_nodes;
    QVector<QPair<int, int>> m_connections;
    QVector<int> m_visible;
    QRectF m_bounds;
    QString m_layout = "Horizontal", m_spacing = "Standard", m_branchStyle = "Rounded", m_error;
    QString m_themeId = "lab";
    bool m_manual = false;
    int m_selected = 1, m_nextId = 1;
    QSet<int> m_selection{1};
    QVector<State> m_undo, m_redo;
    double m_layoutMs = 0;
};
