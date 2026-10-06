// Compare product link dependencies (MiaCode and the miacode_* libraries it
// links) with docs/ops/DEPENDENCY_ALLOWLIST.md.
// Check allowed libraries, forbidden libraries, the pinned Qt version,
// and the source boundary of QtAVPlayer's private media headers.

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>

#ifndef MIACODE_SOURCE_ROOT
#error "MIACODE_SOURCE_ROOT must be defined (repo root absolute path)"
#endif

namespace {

QTextStream& err()
{
    static QTextStream stream(stderr);
    return stream;
}

bool require(bool condition, const QString& message)
{
    if (!condition) {
        err() << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

QString readFile(const QString& relativePath)
{
    QFile file(QStringLiteral(MIACODE_SOURCE_ROOT) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

// Drop `# …` trailing comments; CMake has no block comments and none of the
// link tokens below contain a literal '#'.
QString stripComments(const QString& text)
{
    QStringList kept;
    const QStringList lines = text.split(QLatin1Char('\n'));
    kept.reserve(lines.size());
    for (const QString& line : lines) {
        const qsizetype hash = line.indexOf(QLatin1Char('#'));
        kept.append(hash >= 0 ? line.left(hash) : line);
    }
    return kept.join(QLatin1Char('\n'));
}

// Split a CMake argument list, keeping "quoted strings with spaces"
// (`"-framework AppKit"`) as one token.
QStringList tokenizeArguments(const QString& body)
{
    QStringList tokens;
    QString current;
    bool inQuotes = false;
    for (const QChar ch : body) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            continue;
        }
        current.append(ch);
    }
    if (!current.isEmpty()) {
        tokens.append(current);
    }
    return tokens;
}

// Map a raw link token onto the logical name the doc uses.
//   "…/third_party/bass/lib/win64/bass.lib"        -> bass
//   "…/bass/lib/macos/universal/libbassmix.dylib"  -> bassmix
//   "-framework AppKit"                            -> -framework AppKit
//   Qt6::Quick / soundtouch / dwmapi               -> unchanged
// A CMake variable reference that survives as a whole token (a link list held
// in a variable) is kept verbatim and documented under that name.
QString normalizeLinkToken(const QString& raw)
{
    const QString token = raw.trimmed();
    if (token.startsWith(QLatin1String("-framework "))) {
        return token;
    }
    if (token.startsWith(QLatin1String("${")) && token.endsWith(QLatin1Char('}'))) {
        return token;
    }
    if (!token.contains(QLatin1Char('/'))) {
        return token;
    }
    QString base = token.section(QLatin1Char('/'), -1);
    const qsizetype dot = base.indexOf(QLatin1Char('.'));
    if (dot > 0) {
        base = base.left(dot);
    }
    if (base.startsWith(QLatin1String("lib"))) {
        base = base.mid(3);
    }
    return base;
}

// The MiaCode link closure: every target_link_libraries() call on MiaCode or
// one of its miacode_* libraries, plus the PUBLIC/PRIVATE items of each
// miacode_add_module() call, normalized. The libraries themselves (miacode_*,
// including their generated QML plugin targets) are internal and not listed.
// Paren counting is safe here: no link token contains a parenthesis.
bool isInternalTarget(const QString& token)
{
    return token == QLatin1String("MiaCode") || token.startsWith(QLatin1String("miacode_"));
}

QSet<QString> collectMiaCodeLinkLibraries(const QString& cmake, bool* parsed)
{
    QSet<QString> libraries;
    const QString source = stripComments(cmake);
    static const QRegularExpression call(
        QStringLiteral("(target_link_libraries\\s*\\(\\s*(MiaCode|miacode_\\w+)\\b)"
                       "|(miacode_add_module\\s*\\()"));
    auto it = call.globalMatch(source);
    bool sawAny = false;
    while (it.hasNext()) {
        const auto match = it.next();
        qsizetype cursor = source.indexOf(QLatin1Char('('), match.capturedStart());
        if (cursor < 0) {
            continue;
        }
        int depth = 0;
        qsizetype end = -1;
        for (qsizetype i = cursor; i < source.size(); ++i) {
            if (source.at(i) == QLatin1Char('(')) {
                ++depth;
            } else if (source.at(i) == QLatin1Char(')')) {
                --depth;
                if (depth == 0) {
                    end = i;
                    break;
                }
            }
        }
        if (end < 0) {
            continue;
        }
        sawAny = true;
        const QString body = source.mid(cursor + 1, end - cursor - 1);
        const bool moduleCall = !match.captured(3).isEmpty();
        bool inLinkSection = !moduleCall;
        for (const QString& token : tokenizeArguments(body)) {
            if (token == QLatin1String("PRIVATE") || token == QLatin1String("PUBLIC")
                || token == QLatin1String("INTERFACE")) {
                inLinkSection = true;
                continue;
            }
            if (moduleCall
                && (token == QLatin1String("SOURCES") || token == QLatin1String("QRC"))) {
                inLinkSection = false;
                continue;
            }
            if (!inLinkSection || isInternalTarget(token)) {
                continue;
            }
            libraries.insert(normalizeLinkToken(token));
        }
    }
    if (parsed != nullptr) {
        *parsed = sawAny;
    }
    return libraries;
}

// The first `backticked` cell of every table row inside one `## …` section.
QSet<QString> collectDocSectionEntries(const QString& doc, const QString& heading)
{
    QSet<QString> entries;
    const qsizetype start = doc.indexOf(heading);
    if (start < 0) {
        return entries;
    }
    qsizetype end = doc.indexOf(QStringLiteral("\n## "), start + heading.size());
    if (end < 0) {
        end = doc.size();
    }
    const QString section = doc.mid(start, end - start);
    static const QRegularExpression row(QStringLiteral("^\\|\\s*`([^`]+)`\\s*\\|"),
                                        QRegularExpression::MultilineOption);
    auto it = row.globalMatch(section);
    while (it.hasNext()) {
        entries.insert(it.next().captured(1).trimmed());
    }
    return entries;
}

QStringList sorted(const QSet<QString>& values)
{
    QStringList list(values.cbegin(), values.cend());
    list.sort();
    return list;
}

bool requireSetEquals(const QSet<QString>& actual,
                      const QSet<QString>& documented,
                      const QString& what)
{
    const QSet<QString> undocumented = actual - documented;
    const QSet<QString> stale = documented - actual;
    bool ok = require(undocumented.isEmpty(),
                      what + QStringLiteral(": undocumented -> ")
                          + sorted(undocumented).join(QStringLiteral(", ")));
    ok &= require(stale.isEmpty(),
                  what + QStringLiteral(": documented but not linked -> ")
                      + sorted(stale).join(QStringLiteral(", ")));
    return ok;
}

}  // namespace

int main()
{
    bool ok = true;

    const QString cmake = readFile(QStringLiteral("CMakeLists.txt"));
    const QString modules = readFile(QStringLiteral("cmake/MiaCodeModules.cmake"));
    const QString doc = readFile(QStringLiteral("docs/ops/DEPENDENCY_ALLOWLIST.md"));
    ok &= require(!cmake.isEmpty(), QStringLiteral("CMakeLists.txt is readable"));
    ok &= require(!modules.isEmpty(), QStringLiteral("cmake/MiaCodeModules.cmake is readable"));
    ok &= require(!doc.isEmpty(),
                  QStringLiteral("docs/ops/DEPENDENCY_ALLOWLIST.md is readable"));
    if (!ok) {
        return 1;
    }

    bool parsedLinkCalls = false;
    const QSet<QString> linked =
        collectMiaCodeLinkLibraries(cmake + QLatin1Char('\n') + modules, &parsedLinkCalls);
    ok &= require(parsedLinkCalls,
                  QStringLiteral("MiaCode and library link calls are parseable"));

    const QSet<QString> allowed =
        collectDocSectionEntries(doc, QStringLiteral("## 允许链接进 `MiaCode`"));
    ok &= require(!allowed.isEmpty(),
                  QStringLiteral("the allowlist table has rows"));
    ok &= requireSetEquals(linked, allowed, QStringLiteral("MiaCode link libraries"));

    const QSet<QString> forbidden =
        collectDocSectionEntries(doc, QStringLiteral("## 禁止链接进 `MiaCode`"));
    ok &= require(!forbidden.isEmpty(),
                  QStringLiteral("the forbidden table has rows"));
    for (const QString& name : sorted(forbidden)) {
        ok &= require(!linked.contains(name),
                      QStringLiteral("forbidden dependency is not linked into MiaCode: ")
                          + name);
        ok &= require(!allowed.contains(name),
                      QStringLiteral("dependency is not both allowed and forbidden: ")
                          + name);
    }

    // A REQUIRED find_package component that nothing links is an undeclared
    // build dependency: it makes the build fail on machines that would
    // otherwise be fine. Every product-scope component must therefore either
    // be linked (and documented above) or be declared build-time-only.
    const QSet<QString> buildTimeOnly =
        collectDocSectionEntries(doc, QStringLiteral("## 构建期组件"));
    ok &= require(!buildTimeOnly.isEmpty(),
                  QStringLiteral("the build-time component table has rows"));
    static const QRegularExpression productComponents(
        QStringLiteral("^find_package\\s*\\(\\s*Qt6\\s+[0-9.]+\\s+REQUIRED\\s+COMPONENTS([^)]*)\\)"),
        QRegularExpression::MultilineOption);
    const auto componentsMatch = productComponents.match(stripComments(cmake));
    ok &= require(componentsMatch.hasMatch(),
                  QStringLiteral("the product-scope find_package(Qt6 …) call is parseable"));
    if (componentsMatch.hasMatch()) {
        for (const QString& component : tokenizeArguments(componentsMatch.captured(1))) {
            const QString target = QStringLiteral("Qt6::") + component;
            ok &= require(allowed.contains(target) || buildTimeOnly.contains(component),
                          QStringLiteral("Qt6 component is linked-and-documented or "
                                         "declared build-time-only: ")
                              + component);
        }
    }

    // Stage 3.5: the private Multimedia module must be version-locked, so the
    // doc pins one Qt version and every find_package(Qt6 …) must use it.
    static const QRegularExpression pinned(
        QStringLiteral("Qt 最低版本锁定：`([0-9]+\\.[0-9]+)`"));
    const auto pinnedMatch = pinned.match(doc);
    ok &= require(pinnedMatch.hasMatch(),
                  QStringLiteral("the doc pins a Qt minimum version"));
    if (pinnedMatch.hasMatch()) {
        const QString version = pinnedMatch.captured(1);
        static const QRegularExpression findPackage(
            QStringLiteral("find_package\\s*\\(\\s*Qt6\\s+([0-9]+\\.[0-9]+)"));
        auto it = findPackage.globalMatch(stripComments(cmake));
        int calls = 0;
        while (it.hasNext()) {
            ++calls;
            const QString found = it.next().captured(1);
            ok &= require(found == version,
                          QStringLiteral("find_package(Qt6 %1) matches the pinned %2")
                              .arg(found, version));
        }
        ok &= require(calls > 0,
                      QStringLiteral("CMakeLists.txt has versioned find_package(Qt6 …) calls"));
    }

    // Stage 3.5: QtAVPlayer is the only reason Qt6::MultimediaQuickPrivate is
    // linked. Keep its headers inside the media adapter layer the doc names.
    const QSet<QString> mediaAdapter =
        collectDocSectionEntries(doc, QStringLiteral("## QtAVPlayer 媒体适配层"));
    ok &= require(!mediaAdapter.isEmpty(),
                  QStringLiteral("the media adapter table has rows"));
    QSet<QString> includers;
    QDirIterator walk(QStringLiteral(MIACODE_SOURCE_ROOT) + QStringLiteral("/src"),
                      QStringList{QStringLiteral("*.cpp"), QStringLiteral("*.h"),
                                  QStringLiteral("*.mm"), QStringLiteral("*.inc")},
                      QDir::Files, QDirIterator::Subdirectories);
    const QString root = QStringLiteral(MIACODE_SOURCE_ROOT) + QLatin1Char('/');
    while (walk.hasNext()) {
        const QString path = walk.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll());
        // Assembled at runtime so this spec's own source does not contain the
        // literal it scans for and match itself.
        static const QString needle =
            QStringLiteral("#include <") + QStringLiteral("QtAVPlayer/");
        if (text.contains(needle)) {
            includers.insert(QString(path).remove(0, root.size()));
        }
    }
    ok &= requireSetEquals(includers, mediaAdapter,
                           QStringLiteral("QtAVPlayer header includers"));

    if (ok) {
        QTextStream(stdout) << "dependency_allowlist_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
