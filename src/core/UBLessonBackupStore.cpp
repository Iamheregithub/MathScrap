// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonBackupStore.h"
#include <QtCore>
namespace {
QJsonObject readObject(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
bool writeObject(const QString &path, const QJsonObject &value) {
    QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;
    const auto bytes=QJsonDocument(value).toJson();return f.write(bytes)==bytes.size() && f.commit();
}
QString shaFile(const QString &path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};
    QCryptographicHash h(QCryptographicHash::Sha256);if(!h.addData(&f))return {};
    return QString::fromLatin1(h.result().toHex());
}
bool atomicCopy(const QString &from,const QString &to) {
    QFile input(from);QSaveFile output(to);
    if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))return false;
    while(!input.atEnd()){auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFile::NoError)return false;if(output.write(bytes)!=bytes.size())return false;}
    return output.commit();
}
bool validId(const QString &id){return QRegularExpression("^[0-9a-fA-F-]{36}$").match(id).hasMatch();}
QJsonArray versions(const QString &root,const QString &id) {
    QJsonArray result;if(!validId(id))return result;
    const QString folder=root+"/"+id;auto latest=readObject(folder+"/recovery.json");if(!latest.isEmpty()){latest["file"]="recovery.json";result.append(latest);}
    QDir versions(folder+"/versions");for(const auto &name:versions.entryList({"*.json"},QDir::Files,QDir::Name|QDir::Reversed)){
        auto entry=readObject(versions.filePath(name));if(!entry.isEmpty()){entry["file"]="versions/"+name;result.append(entry);}
    }return result;
}
void prune(const QString &root,const QString &id) {
    const QString folder=root+"/"+id;QDir dir(folder+"/versions");auto names=dir.entryList({"*.json"},QDir::Files,QDir::Name|QDir::Reversed);
    for(int i=50;i<names.size();++i)QFile::remove(dir.filePath(names[i]));
    QSet<QString> needed;for(const auto &v:versions(root,id))for(const auto &f:v.toObject().value("files").toArray())needed.insert(f.toObject().value("sha256").toString());
    QDir blobs(folder+"/blobs");for(const auto &name:blobs.entryList(QDir::Files))if(QRegularExpression("^[a-f0-9]{64}$").match(name).hasMatch()&&!needed.contains(name))QFile::remove(blobs.filePath(name));
}
}
UBLessonBackupResult UBLessonBackupStore::save(const UBLessonBackupJob &job) {
    UBLessonBackupResult result;result.id=job.id;
    const QString id=job.id;
    auto fail=[&](const QString &message){result.error=message;return result;};
    auto sourceChanged=[&]{result.sourceChanged=true;return fail(QObject::tr("The lesson changed during recovery saving; it will be retried."));};
    if(!validId(id))return fail(QObject::tr("The lesson has no valid identifier."));
    const QString folder=job.root+"/"+id, blobs=folder+"/blobs";
    const QDir source(job.source);QDirIterator iterator(source.absolutePath(),QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    QStringList paths;while(iterator.hasNext()){iterator.next();if(iterator.fileInfo().isSymLink())return fail(QObject::tr("Linked files cannot be included in recovery copies."));if(!iterator.fileName().contains(".thumbnail.")&&!iterator.fileName().startsWith('.'))paths<<source.relativeFilePath(iterator.filePath());}paths.sort();
    bool hasPage=false;
    for(const auto &path:paths)hasPage|=QRegularExpression("^page[0-9]+\\.svg$").match(path).hasMatch();
    if(!hasPage||!paths.contains("metadata.rdf"))return sourceChanged();
    QJsonArray entries;QCryptographicHash content(QCryptographicHash::Sha256);
    content.addData(job.name.toUtf8());
    for(const auto &relative:paths){const QString path=source.filePath(relative);
        const QString hash=shaFile(path);
        if(hash.isEmpty())return fail(QObject::tr("Cannot read %1.").arg(relative));
        QJsonObject entry;entry["path"]=relative;entry["sha256"]=hash;entries.append(entry);
        // Metadata contains a save timestamp. It is retained, but does not create redundant versions.
        if(relative!="metadata.rdf"){content.addData(relative.toUtf8());content.addData(hash.toLatin1());}
    }
    const QString digest=QString::fromLatin1(content.result().toHex());
    result.digest=digest;
    if(job.recordDeletion){result.success=true;return result;}
    // Delete all stays empty for unchanged content, including across restarts.
    // New work, or an explicit Create saved version, resumes recovery normally.
    if(!job.force&&job.clearedDigest==digest){result.success=true;result.suppressed=true;return result;}
    if(!QDir().mkpath(blobs)||!QDir().mkpath(folder+"/versions"))return fail(QObject::tr("Cannot create the lesson history folder."));
    for(const auto &value:entries){const auto entry=value.toObject();const auto hash=entry.value("sha256").toString(),relative=entry.value("path").toString();
        if(shaFile(blobs+"/"+hash)!=hash&&(!atomicCopy(source.filePath(relative),blobs+"/"+hash)||shaFile(blobs+"/"+hash)!=hash))return fail(QObject::tr("Could not verify the recovery copy of %1.").arg(relative));
    }
    QStringList after;
    QDirIterator verify(source.absolutePath(),QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(verify.hasNext()) {
        verify.next();
        if(verify.fileInfo().isSymLink())return sourceChanged();
        if(!verify.fileName().contains(".thumbnail.")&&!verify.fileName().startsWith('.'))
            after << source.relativeFilePath(verify.filePath());
    }
    after.sort();
    if(after!=paths)return sourceChanged();
    for(const auto &value:entries) {
        const auto entry=value.toObject();
        if(shaFile(source.filePath(entry.value("path").toString()))!=entry.value("sha256").toString())return sourceChanged();
    }
    auto previous=readObject(folder+"/recovery.json");
    QJsonObject snapshot;snapshot["name"]=job.name;snapshot["id"]=id;snapshot["created"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);snapshot["digest"]=digest;snapshot["files"]=entries;
    const auto oldVersions=versions(job.root,id);QJsonObject last;
    for(const auto &v:oldVersions)if(v.toObject().value("file")!="recovery.json"){last=v.toObject();break;}
    const bool changed=digest!=previous.value("digest").toString();
    const bool versionChanged=digest!=last.value("digest").toString();
    if(versionChanged&&(job.keepVersion||last.isEmpty()||QDateTime::fromString(last.value("created").toString(),Qt::ISODateWithMs).secsTo(QDateTime::currentDateTimeUtc())>=300)){
        const QString name=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
        if(!writeObject(folder+"/versions/"+name,snapshot))return fail(QObject::tr("Cannot commit the saved version."));
    }
    if(changed||previous.isEmpty())if(!writeObject(folder+"/recovery.json",snapshot))return fail(QObject::tr("Cannot commit the recovery copy."));
    prune(job.root,id);
    result.success=true;return result;
}
