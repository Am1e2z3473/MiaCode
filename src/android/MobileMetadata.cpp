#include "AndroidDocumentSession.h"
#include "common/Id3TagReader.h"
#include <QImageReader>
#include <QBuffer>
#include <QDir>
#include <QSaveFile>
#include <QUuid>

namespace miacode::android {
QString AndroidDocumentSession::metadataClockCount() const
{
    for (const auto& field : workspace_.document().extraFields)
        if (field.key.compare("clock_count", Qt::CaseInsensitive) == 0) return field.value;
    return {};
}
QString AndroidDocumentSession::metadataExtraText() const
{
    QVector<SimaiRawField> fields;
    for (const auto& field : workspace_.document().extraFields)
        if (field.key.compare("clock_count", Qt::CaseInsensitive) != 0) fields.append(field);
    return SimaiDocument::serializeRawFields(fields);
}
QString AndroidDocumentSession::metadataAttentionText() const
{
    QStringList fields;
    if (title().trimmed().isEmpty()) fields.append(qtTrId("metadata.field.title"));
    if (metadataArtist().trimmed().isEmpty()) fields.append(qtTrId("metadata.field.artist"));
    if (metadataDesigner().trimmed().isEmpty()) fields.append(qtTrId("metadata.field.des"));
    if (previewAssetPath("image").isEmpty() && previewAssetPath("video").isEmpty())
        fields.append(qtTrId("metadata.field.cover"));
    return fields.isEmpty() ? QString() : qtTrId("metadata.needs_attention").arg(fields.join(QStringLiteral("、")));
}
void AndroidDocumentSession::setMetadataArtist(const QString& value)
{ workspace_.updateDocumentField(ChartWorkspaceDocumentField::Artist, value); }
void AndroidDocumentSession::setMetadataFirst(const QString& value)
{ workspace_.updateDocumentField(ChartWorkspaceDocumentField::First, value); }
void AndroidDocumentSession::setMetadataDesigner(const QString& value)
{ workspace_.updateDocumentField(ChartWorkspaceDocumentField::Designer, value); }
void AndroidDocumentSession::setMetadataClockCount(const QString& value)
{ workspace_.upsertExtraField("clock_count", value); }
void AndroidDocumentSession::setMetadataExtraText(const QString& value)
{
    if (!workspace_.replaceExtraFields(value, metadataClockCount()).accepted) {
        status_ = tr("其他字段格式无效，已保留原有字段。");
        emit changed();
    }
}
QVariantList AndroidDocumentSession::designerSlots() const
{
    QVariantList rows;
    for (int id = 1; id <= 7; ++id) rows.append(QVariantMap{{"id", id},
        {"name", SimaiDocument::difficultyName(id)}, {"designer", workspace_.document().designerForSlot(id)},
        {"hasChart", workspace_.document().difficulty(id) != nullptr}});
    return rows;
}
void AndroidDocumentSession::applyDesignerSlots(const QVariantList& slotValues, bool unified, const QString& name)
{
    // Keep the same workspace operation sequence as v2's DocumentModel.
    workspace_.setUnifiedDesignerEnabled(false);
    for (const auto& slot : slotValues) {
        const auto value = slot.toMap();
        workspace_.setDesignerForSlot(value.value("id").toInt(), value.value("designer").toString());
    }
    if (unified) {
        workspace_.setUnifiedDesignerEnabled(true);
        workspace_.unifyDesigners(name);
    }
    emit metadataChanged();
}
void AndroidDocumentSession::readTitleFromAudioFile()
{
    const auto tag = id3::readTagFromFile(previewAssetPath("audio"));
    if (tag.valid && !tag.title.trimmed().isEmpty()) setTitle(tag.title.trimmed());
    else { status_ = tr("当前音频没有可读取的 ID3v2 标题；可先导入其他音频。"); emit changed(); }
}
void AndroidDocumentSession::readArtistFromAudioFile()
{
    const auto tag = id3::readTagFromFile(previewAssetPath("audio"));
    if (tag.valid && !tag.artist.trimmed().isEmpty()) setMetadataArtist(tag.artist.trimmed());
    else { status_ = tr("当前音频没有可读取的 ID3v2 艺术家信息；可先导入其他音频。"); emit changed(); }
}
void AndroidDocumentSession::extractCoverFromAudioFile()
{
    if (busy()) return;
    const auto tag = id3::readTagFromFile(previewAssetPath("audio"));
    QByteArray bytes = tag.pictureBytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    if (!tag.valid || bytes.isEmpty() || !reader.canRead()) {
        status_ = tr("当前音频没有可读取的内嵌封面。"); emit changed(); return;
    }
    const QString folder = storageRoot_ + "/imports";
    const QString path = folder + '/' + QUuid::createUuid().toString(QUuid::WithoutBraces)
        + '.' + QString::fromLatin1(reader.format());
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!QDir().mkpath(folder) || !output.open(QIODevice::WriteOnly)
        || output.write(bytes) != bytes.size() || !output.commit()) {
        status_ = tr("无法保存内嵌封面：%1").arg(output.errorString()); emit changed(); return;
    }
    assets_.append(QVariantMap{{"kind", "image"}, {"name", "Embedded cover"},
        {"path", path}, {"selectedForPreview", true}});
    emit mediaAssetsChanged();
    emit metadataChanged();
    status_ = tr("已从音频提取封面，原有素材保留在本机。");
    flushRecovery();
}
void AndroidDocumentSession::removeChartPv()
{
    videoDisabled_ = true;
    workspace_.updateDocumentField(ChartWorkspaceDocumentField::VideoPath, {});
    emit mediaAssetsChanged();
    emit metadataChanged();
    status_ = tr("已移除当前预览的视频背景，本机素材副本保留。");
    flushRecovery();
}
}
