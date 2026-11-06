/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */



#include <QDir>
#include <QList>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include "core/UBApplication.h"
#include "core/UBPersistenceManager.h"
#include "core/UBDocumentManager.h"
#include "core/UBPersistenceManager.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBGraphicsPDFItem.h"
#include "frameworks/UBFileSystemUtils.h"
#include "pdf/PDFRenderer.h"

#include "UBCFFSubsetAdaptor.h"
#include "UBImportCFF.h"

#include "globals/UBGlobals.h"

#include "core/memcheck.h"

UBImportCFF::UBImportCFF(QObject *parent)
    : UBDocumentBasedImportAdaptor(parent)
{
    // NOOP
}


UBImportCFF::~UBImportCFF()
{
    // NOOP
}


QStringList UBImportCFF::supportedExtentions()
{
    return QStringList("iwb");
}


QString UBImportCFF::importFileFilter()
{
    QString filter = tr("Common File Format (");
    QStringList formats = supportedExtentions();
    bool isFirst = true;

    foreach(QString format, formats)
    {
            if(isFirst)
                    isFirst = false;
            else
                    filter.append(" ");

        filter.append("*."+format);
    }

    filter.append(")");

    return filter;
}

namespace
{
QString cffContentFile(const QFile &file, const QTemporaryDir &temporary)
{
    // Standalone CFF XML is supported alongside packaged IWB files.
    QFile source(file.fileName());
    if (source.open(QIODevice::ReadOnly))
    {
        QXmlStreamReader xml(&source);
        if (xml.readNextStartElement())
            return file.fileName();
    }

    if (!temporary.isValid() || !UBFileSystemUtils::expandZipToDir(file, QDir(temporary.path())))
        return {};
    return QDir(temporary.path()).filePath("content.xml");
}
}

bool UBImportCFF::addFileToDocument(std::shared_ptr<UBDocumentProxy> pDocument, const QFile& pFile)
{
    QFileInfo fi(pFile);
    UBApplication::showMessage(tr("Importing file %1...").arg(fi.baseName()), true);

    QTemporaryDir temporary;
    QString contentFile = cffContentFile(pFile, temporary);

    if(!contentFile.length()){
            UBApplication::showMessage(tr("Import of file %1 failed.").arg(fi.baseName()));
            return false;
    }
    else{
        //TODO convert expanded CFF file content to the destination document
        //create destination document proxy
        //fill metadata and save
        std::shared_ptr<UBDocumentProxy> destDocument = std::make_shared<UBDocumentProxy>(UBPersistenceManager::persistenceManager()->generateUniqueDocumentPath());
        QDir dir;
        dir.mkdir(destDocument->persistencePath());

        //try to import cff to document
        if (UBCFFSubsetAdaptor::ConvertCFFFileToUbz(contentFile, destDocument) &&
            UBPersistenceManager::persistenceManager()->addDirectoryContentToDocument(destDocument->persistencePath(), pDocument))
        {
            UBFileSystemUtils::deleteDir(destDocument->persistencePath());
            UBApplication::showMessage(tr("Import successful."));
            return true;
        }
        else
        {
            UBFileSystemUtils::deleteDir(destDocument->persistencePath());
            UBApplication::showMessage(tr("Import failed."));
            return false;
        }
    }
}

std::shared_ptr<UBDocumentProxy> UBImportCFF::importFile(const QFile& pFile, const QString& pGroup)
{
    Q_UNUSED(pGroup); // group is defined in the imported file

    QFileInfo fi(pFile);
    UBApplication::showMessage(tr("Importing file %1...").arg(fi.baseName()), true);

    QTemporaryDir temporary;
    QString contentFile = cffContentFile(pFile, temporary);

    if(!contentFile.length()){
            UBApplication::showMessage(tr("Import of file %1 failed.").arg(fi.baseName()));
            return 0;
    }
    else{
        //create destination document proxy
        //fill metadata and save
        std::shared_ptr<UBDocumentProxy> destDocument = std::make_shared<UBDocumentProxy>(UBPersistenceManager::persistenceManager()->generateUniqueDocumentPath());
        QDir dir;
        dir.mkdir(destDocument->persistencePath());
        if (pGroup.length() > 0)
            destDocument->setMetaData(UBSettings::documentGroupName, pGroup);
        if (!fi.baseName().isEmpty())
            destDocument->setMetaData(UBSettings::documentName, fi.baseName());

        destDocument->setMetaData(UBSettings::documentVersion, UBSettings::currentFileVersion);
        destDocument->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(QDateTime::currentDateTime()));

        std::shared_ptr<UBDocumentProxy> newDocument = nullptr;
        //try to import cff to document
        if (UBCFFSubsetAdaptor::ConvertCFFFileToUbz(contentFile, destDocument))
        {
            newDocument = UBPersistenceManager::persistenceManager()->createDocumentFromDir(destDocument->persistencePath()
                                                                                            ,""
                                                                                            ,""
                                                                                            ,false
                                                                                            ,false
                                                                                            ,true);

            UBApplication::showMessage(tr("Import successful."));
        }
        else
        {
            UBFileSystemUtils::deleteDir(destDocument->persistencePath());
            UBApplication::showMessage(tr("Import failed."));
        }

        return newDocument;
    }
}
