#pragma once
#include <QString>
#include <QStringList>
#include <QFontDatabase>

// Use the host's UI sans serif; keep measurement, canvas, export and editor identical.
inline QString mindarchyTextFamily() {
#ifdef Q_OS_WIN
    return QStringLiteral("Segoe UI");
#elif defined(Q_OS_MACOS)
    // Explicit family also resolves in offscreen Quick Look and export processes.
    return QStringLiteral("Helvetica Neue");
#else
    const auto families=QFontDatabase::families();
    for(const auto &name : {QStringLiteral("Inter"),QStringLiteral("Noto Sans"),QStringLiteral("DejaVu Sans")})
        if(families.contains(name)) return name;
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
}

// Shared by measurement, canvas, editing and export. Explicit HTML sizes never
// override the hierarchy; all other character formatting is preserved.
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
inline int mindarchyNodeFontSize(int depth) {
    return depth <= 0 ? 20 : qMax(14, 19 - depth);
}
inline void applyMindarchyNodeSize(QTextDocument &doc, int size) {
    QFont base=doc.defaultFont(); base.setPixelSize(size); doc.setDefaultFont(base);
    QVector<QPair<QTextCursor,QTextCharFormat>> changes;
    for(auto block=doc.begin();block.isValid();block=block.next()) {
        QTextCursor blockCursor(block);
        auto blockChar=block.charFormat();
        blockChar.clearProperty(QTextFormat::FontPointSize);
        blockChar.clearProperty(QTextFormat::FontSizeAdjustment);
        blockChar.setProperty(QTextFormat::FontPixelSize,size);
        if(blockChar!=block.charFormat()) blockCursor.setBlockCharFormat(blockChar);
        for(auto it=block.begin();!it.atEnd();++it) {
            const auto fragment=it.fragment(); auto format=fragment.charFormat();
            format.clearProperty(QTextFormat::FontPointSize);
            format.clearProperty(QTextFormat::FontSizeAdjustment);
            format.setProperty(QTextFormat::FontPixelSize,size);
            QTextCursor cursor(&doc); cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);
            if(format!=fragment.charFormat()) changes.append({cursor,format});
        }
    }
    for(auto &change:changes) change.first.setCharFormat(change.second);
}
