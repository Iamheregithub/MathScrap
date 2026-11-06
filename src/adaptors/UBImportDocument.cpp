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




#include "UBImportDocument.h"

#include <QTemporaryDir>
#include "document/UBDocumentProxy.h"

#include "frameworks/UBFileSystemUtils.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBPersistenceManager.h"

#include "globals/UBGlobals.h"

#include "core/memcheck.h"

UBImportDocument::UBImportDocument(QObject *parent)
    :UBDocumentBasedImportAdaptor(parent)
{
    // NOOP
}

UBImportDocument::~UBImportDocument()
{
    // NOOP
}


QStringList UBImportDocument::supportedExtentions()
{
    return QStringList("ubz");
}


QString UBImportDocument::importFileFilter()
{
    return tr("OpenBoard (*.ubz)");
}


bool UBImportDocument::extractFileToDir(const QFile& pZipFile, const QString& pDir, QString& documentRoot)
{
    const QString path = UBPersistenceManager::persistenceManager()->generateUniqueDocumentPath(pDir);
    if (!QDir().mkdir(path))
        return false;

    if (!UBFileSystemUtils::expandZipToDir(pZipFile, QDir(path)))
    {
        UBFileSystemUtils::deleteDir(path);
        return false;
    }

    documentRoot = path;
    return true;
}


std::shared_ptr<UBDocumentProxy> UBImportDocument::importFile(const QFile& pFile, const QString& pGroup)
{
    Q_UNUSED(pGroup); // group is defined in the imported file

    QFileInfo fi(pFile);
    UBApplication::showMessage(tr("Importing file %1...").arg(fi.baseName()), true);

    // first unzip the file to the correct place
    QString path = UBSettings::userDocumentDirectory();

    QString documentRootFolder;

    if(!extractFileToDir(pFile, path, documentRootFolder)){
        UBApplication::showMessage(tr("Import of file %1 failed.").arg(fi.baseName()));
        return NULL;
    }

    std::shared_ptr<UBDocumentProxy> newDocument = UBPersistenceManager::persistenceManager()->createDocumentFromDir(documentRootFolder, pGroup, "", false, false, true);

    UBApplication::showMessage(tr("Import successful."));

    return newDocument;
}

bool UBImportDocument::addFileToDocument(std::shared_ptr<UBDocumentProxy> pDocument, const QFile& pFile)
{
    QFileInfo fi(pFile);
    UBApplication::showMessage(tr("Importing file %1...").arg(fi.baseName()), true);

    QTemporaryDir temporary;
    if (!temporary.isValid())
        return false;
    const QString path = temporary.path();

    QString documentRootFolder;
    if (!extractFileToDir(pFile, path, documentRootFolder))
    {
        UBApplication::showMessage(tr("Import of file %1 failed.").arg(fi.baseName()));
        return false;
    }

    if (!UBPersistenceManager::persistenceManager()->addDirectoryContentToDocument(documentRootFolder, pDocument))
    {
        UBApplication::showMessage(tr("Import of file %1 failed.").arg(fi.baseName()));
        return false;
    }

    UBApplication::showMessage(tr("Import successful."));

    return true;
}
