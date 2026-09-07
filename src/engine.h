#pragma once
#include <QHash>
#include <QObject>
#include <QRectF>
#include <QSet>
#include <QVariantList>
#include <QVector>
#include "theme.h"

struct MapNode {
    int id = 0, parent = -1;
    QVector<int> children;
    QString text, notes;
    QVariantMap style;
    bool folded = false, task = false, checked = false;
    QPointF manualOffset;
    QRectF rect;
    int depth = 0;
};
class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY changed)
    Q_PROPERTY(QString spacing READ spacing WRITE setSpacing NOTIFY changed)
    Q_PROPERTY(QString branchStyle READ branchStyle WRITE setBranchStyle NOTIFY changed)
    Q_PROPERTY(bool manual READ manual WRITE setManual NOTIFY changed)
    Q_PROPERTY(int selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QVariantList selection READ selection NOTIFY changed)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY outlineChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY changed)
    Q_PROPERTY(QString selectedNotes READ selectedNotes NOTIFY changed)
    Q_PROPERTY(bool selectedTask READ selectedTask NOTIFY changed)
    Q_PROPERTY(bool selectedChecked READ selectedChecked NOTIFY changed)
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
    Q_PROPERTY(QVariantMap selectedStyle READ selectedStyle NOTIFY changed)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    Q_PROPERTY(QColor canvasColor READ canvasColor NOTIFY changed)
  public:
    explicit Engine(QObject *parent = nullptr);
    const QHash<int, MapNode> &nodes() const { return m_nodes; }
    const QVector<int> &visibleIds() const { return m_visible; }
    QRectF bounds() const { return m_bounds; }
    QHash<int,QRectF> manualGeometry(int movingId = -1, QPointF delta = {}) const;
    QSet<int> selectedIds() const { return m_selection; }
    bool isDescendant(int node, int ancestor) const;
    QString layout() const { return m_layout; }
    QString spacing() const { return m_spacing; }
    QString branchStyle() const { return m_branchStyle; }
    bool manual() const { return m_manual; }
    int selectedId() const { return m_selected; }
    QVariantList selection() const;
    QVariantList outline() const;
    QString selectedText() const { return m_nodes.value(m_selected).text; }
    QString selectedNotes() const { return m_nodes.value(m_selected).notes; }
    bool selectedTask() const { return m_nodes.value(m_selected).task; }
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
    Q_INVOKABLE void addChild();
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
    Q_INVOKABLE void moveManual(int id, double dx, double dy);
    Q_INVOKABLE bool save(QString path);
    Q_INVOKABLE bool open(QString path);
  signals:
    void changed();
    void outlineChanged();
    void editRequested(int id);

  private:
    struct State {
        QHash<int, MapNode> nodes;
        QVector<QPair<int, int>> connections;
        QString layout, spacing, branchStyle, themeId;
        bool manual;
        int selected, nextId;
        QSet<int> selection;
    };
    State state() const;
    void restore(const State &state);
    void checkpoint();
    void rebuild();
    void add(int parent, int after = -1);
    bool fail(const QString &error);
    // One entry per current node; font15, zero document margin and renderer padding
    // are fixed measurement constants. Text and task changes replace the entry.
    struct TextMeasure {
        QString text, plainText;
        bool task;
        QSizeF size;
        double width = 0;
    };
    static bool measureText(const QString &text, bool task, TextMeasure &result, double fixedWidth = 0);
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
