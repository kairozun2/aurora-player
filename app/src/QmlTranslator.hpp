// Aurora Player - runtime RU/EN switching for the QML layer.
//
// Instead of pre-compiled .qm catalogues we install a QTranslator that resolves
// qsTr()/tr() source strings through a plain JSON dictionary. That means the
// language can be switched instantly at runtime (QQmlEngine::retranslate()
// re-evaluates every binding), translators can edit i18n/ru.json without any
// Qt tooling, and the same dictionary works for C++ and QML strings.
#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTranslator>

namespace aurora {

class QmlTranslator : public QTranslator {
public:
    explicit QmlTranslator(QObject* parent = nullptr) : QTranslator(parent) {}

    /// Loads a { "source string": "translation" } dictionary.
    bool loadDictionary(const QString& path) {
        dict_.clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;

        const QJsonObject object = document.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const QString value = it.value().toString();
            if (!value.isEmpty()) dict_.insert(it.key(), value);
        }
        return !dict_.isEmpty();
    }

    bool isEmpty() const override { return dict_.isEmpty(); }

    QString translate(const char* context, const char* sourceText, const char* disambiguation,
                      int n) const override {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)
        if (sourceText == nullptr) return QString();
        // Context is intentionally ignored: one flat dictionary for the whole app.
        return dict_.value(QString::fromUtf8(sourceText));
    }

    /// Looks for the dictionary in the resources, next to the binary, and in the
    /// source tree, so it works both installed and straight out of a build dir.
    static QString findDictionary(const QString& code) {
        const QString name = QStringLiteral("%1.json").arg(code);
        QStringList candidates;
        candidates << QStringLiteral(":/i18n/") + name;
        const QString appDir = QCoreApplication::applicationDirPath();
        candidates << appDir + QStringLiteral("/i18n/") + name;
        candidates << appDir + QStringLiteral("/../share/aurora-player/i18n/") + name;
        candidates << appDir + QStringLiteral("/../Resources/i18n/") + name;
#ifdef AURORA_SOURCE_DIR
        candidates << QStringLiteral(AURORA_SOURCE_DIR) + QStringLiteral("/i18n/") + name;
#endif
        for (const QString& candidate : candidates) {
            if (QFile::exists(candidate)) return candidate;
        }
        return QString();
    }

private:
    QHash<QString, QString> dict_;
};

} // namespace aurora
