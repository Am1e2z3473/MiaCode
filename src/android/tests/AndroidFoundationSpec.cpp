#include "android/AndroidDocumentSession.h"
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QDir>
#include <QTemporaryDir>
#include <cstdio>

using miacode::android::AndroidDocumentSession;
namespace {
int failures = 0;
void check(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
}
int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary storage exists");
    const QString uri = "content://documents/chart";
    QString savedText;
    {
        AndroidDocumentSession session(directory.path());
        session.setTitle(QString::fromUtf8("手机与平板 · 回归"));
        session.setChartText("(180){4}1,2,3,4,E");
        check(session.dirty(), "chart edit becomes dirty");
        session.addDifficulty(6);
        session.selectDifficulty(6);
        session.setChartText("(200){8}8,7,6,5,E");
        session.selectDifficulty(5);
        session.save(true);
        check(session.busy(), "save request is pending");
        savedText = session.workspace().snapshot().sourceText;
        session.setChartText("(180){4}1,2,3,4,5,E");
        session.completeIo({{"kind", "saveAs"}, {"ok", true}, {"uri", uri}});
        check(session.dirty(), "late save completion preserves newer edits");
        check(session.sourceUri() == uri, "source URI survives save");
        check(session.flushRecovery(), "atomic recovery can be written");
        check(QFile::exists(session.workspace().snapshot().filePath), "internal maidata working copy exists");
    }
    {
        AndroidDocumentSession session(directory.path());
        check(session.recoveryAvailable(), "recovery discovered without overwriting it");
        check(session.recover(), "process restart recovers document");
        check(session.dirty(), "recovery retains original save point");
        check(session.chartText().contains("4,5,E"), "recovery retains latest edit");
        check(session.difficulties().size() == 2, "all difficulties recover");
        session.selectDifficulty(6);
        check(session.chartText().contains("8,7,6,5"), "inactive difficulty survives restart");
        session.save();
        session.completeIo({{"kind", "save"}, {"ok", false}, {"error", "permission revoked"}});
        check(session.dirty() && session.sourceUri() == uri, "failed provider write preserves dirty state and origin");
        session.save();
        session.completeIo({{"kind", "open"}, {"ok", true}});
        check(session.busy(), "unrelated completion cannot advance save point");
        session.completeIo({{"kind", "save"}, {"ok", true}, {"uri", uri}});
        check(!session.dirty(), "successful exact snapshot save clears dirty state");
        session.setBackgroundExportAllowed(true);
        check(session.flushRecovery(), "background preference persists");
    }
    {
        AndroidDocumentSession session(directory.path());
        check(session.recover() && session.backgroundExportAllowed(), "user's background choice survives restart");
        session.openProject();
        session.completeIo({{"kind", "open"}, {"ok", true}, {"uri", "content://bad"},
            {"data", QString::fromLatin1(QByteArray("\xff\xfe", 2).toBase64())}});
        check(session.sourceUri() == uri, "invalid UTF-8 does not replace current project");
        session.openProject();
        session.completeIo({{"kind", "open"}, {"ok", false}, {"cancelled", true}});
        check(session.sourceUri() == uri, "picker cancellation preserves project");
    }
    QTemporaryDir blocked;
    const QString invalidRoot = blocked.path() + "/regular-file";
    QFile file(invalidRoot);
    check(file.open(QIODevice::WriteOnly), "failure fixture created");
    file.close();
    AndroidDocumentSession session(invalidRoot);
    session.setChartText("(120){4}1,2,E");
    check(!session.flushRecovery(), "unwritable storage is reported");
    check(session.dirty(), "storage failure does not discard edits");
    session.save(true);
    check(!session.busy(), "unsafe save blocked until local recovery succeeds");
    QTemporaryDir fixtureRoot;
    AndroidDocumentSession fixtures(fixtureRoot.path());
    QFile multi(QString::fromUtf8(MIACODE_ANDROID_FIXTURE_DIR) + "/multidifficulty-maidata.txt");
    check(multi.open(QIODevice::ReadOnly), "multi-difficulty fixture can be read");
    fixtures.openProject();
    fixtures.completeIo({{"kind", "open"}, {"ok", true}, {"uri", uri},
        {"data", QString::fromLatin1(multi.readAll().toBase64())}});
    check(fixtures.difficulties().size() == 3, "fixture keeps all three difficulty slots");
    check(fixtures.workspace().document().videoPath == "media/background.mp4", "relative PV metadata is preserved");
    check(fixtures.workspace().snapshot().sourceText.contains("&custom_android_fixture=preserve unknown metadata"),
        "unmanaged metadata round trips");
    check(fixtures.workspace().document().designerForSlot(6) == "ReMaster author", "per-difficulty designer round trips");
    QFile incomplete(QString::fromUtf8(MIACODE_ANDROID_FIXTURE_DIR) + "/incomplete-maidata.txt");
    check(incomplete.open(QIODevice::ReadOnly), "incomplete fixture can be read");
    fixtures.openProject();
    fixtures.completeIo({{"kind", "open"}, {"ok", true}, {"uri", uri},
        {"data", QString::fromLatin1(incomplete.readAll().toBase64())}});
    check(fixtures.chartText().contains("2h["), "unfinished syntax is editable and recoverable");
    check(fixtures.flushRecovery(), "unfinished chart recovery writes successfully");
    {
        const QString projectRoot = QFileInfo(fixtures.currentFilePath()).absolutePath();
        const QString defaultAudio = projectRoot + "/track.mp3";
        const QString defaultImage = projectRoot + "/bg.jpg";
        for (const auto& path : {defaultAudio, defaultImage}) {
            QFile asset(path);
            check(asset.open(QIODevice::WriteOnly), "project media fixture created");
            asset.write("media-fixture");
        }
        check(fixtures.previewAssetPath("audio") == defaultAudio, "project audio is resolved");
        check(fixtures.previewAssetPath("image") == defaultImage, "project bg is resolved");
        const QString imported = fixtureRoot.path() + "/replacement.wav";
        QFile replacement(imported);
        check(replacement.open(QIODevice::WriteOnly), "imported media fixture created");
        replacement.write("media-fixture");
        replacement.close();
        const auto revision = fixtures.documentRevision();
        const auto chart = fixtures.chartText();
        int notifications = 0;
        QObject::connect(&fixtures, &AndroidDocumentSession::mediaAssetsChanged, [&] { ++notifications; });
        fixtures.importAsset("audio");
        fixtures.completeIo({{"kind", "asset"}, {"ok", true},
            {"asset", QJsonObject{{"kind", "audio"}, {"path", imported}}}});
        check(notifications == 1, "media import publishes refresh notification");
        check(fixtures.previewAssetPath("audio") == imported, "imported audio replaces project default");
        check(fixtures.documentRevision() == revision && fixtures.chartText() == chart,
            "media import does not mutate chart text or revision");
        AndroidDocumentSession restored(fixtureRoot.path());
        check(restored.recover() && restored.previewAssetPath("audio") == imported,
            "selected media survives process recovery");
        restored.newProject();
        check(restored.previewAssetPath("audio").isEmpty(), "new project cannot inherit previous media");
        restored.setMetadataArtist("Mobile artist");
        restored.setMetadataFirst("-1.250");
        restored.setMetadataClockCount("3");
        restored.setMetadataExtraText("&wholebpm=117\n&custom_metadata=keep\n");
        check(restored.metadataClockCount() == "3" && restored.metadataExtraText().contains("custom_metadata"),
            "extra-field form preserves the dedicated clock-count field");
        check(restored.workspace().document().artist == "Mobile artist" && restored.metadataFirst() == "-1.250",
            "metadata form commits to the sole chart workspace");
        restored.applyDesignerSlots({QVariantMap{{"id", 7}, {"designer", "Chartless author"}}}, false, {});
        check(!restored.workspace().document().difficulty(7)
            && restored.workspace().document().designerForSlot(7) == "Chartless author",
            "designer management preserves chartless slots");
        restored.applyDesignerSlots({}, true, "Shared author");
        restored.setMetadataDesigner("Updated shared author");
        check(restored.unifiedDesignerEnabled() && restored.workspace().document().designerForSlot(5) == "Updated shared author"
            && restored.workspace().document().designerForSlot(7) == "Updated shared author",
            "unified designer edits reach charted and chartless slots");
        const QString pv = QFileInfo(restored.currentFilePath()).absolutePath() + "/pv.mp4";
        QFile video(pv);
        check(QDir().mkpath(QFileInfo(pv).absolutePath()) && video.open(QIODevice::WriteOnly), "PV fixture created");
        video.write("video-fixture");
        video.close();
        check(restored.metadataHasVideo(), "v2 sibling PV fallback resolves without an explicit video field");
        restored.removeChartPv();
        check(!restored.metadataHasVideo() && QFile::exists(pv), "removing PV disables it while preserving local media");
        AndroidDocumentSession removedPv(fixtureRoot.path());
        check(removedPv.recover() && !removedPv.metadataHasVideo(), "removed PV stays disabled after recovery");
    }
    std::printf("AndroidFoundationSpec: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
