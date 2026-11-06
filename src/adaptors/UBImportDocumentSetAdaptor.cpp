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

#include "UBImportDocumentSetAdaptor.h"

#include <QTemporaryDir>

#include "document/UBDocumentProxy.h"

#include "frameworks/UBFileSystemUtils.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBPersistenceManager.h"

#include "globals/UBGlobals.h"

#include "core/memcheck.h"

UBImportDocumentSetAdaptor::UBImportDocumentSetAdaptor(QObject *parent)
    :UBImportAdaptor(parent)
{
    // NOOP
}

UBImportDocumentSetAdaptor::~UBImportDocumentSetAdaptor()
{
    // NOOP
}


QStringList UBImportDocumentSetAdaptor::supportedExtentions()
{
    return QStringList("ubx");
}


QString UBImportDocumentSetAdaptor::importFileFilter()
{
    return tr("Openboard (set of documents) (*.ubx)");
}

QFileInfoList UBImportDocumentSetAdaptor::importData(const QString &zipFile, const QString &destination)
{
    QTemporaryDir temporary;
    if (!temporary.isValid() || !UBFileSystemUtils::expandZipToDir(QFile(zipFile), QDir(temporary.path())))
        return {};

    const QString tmpDir = temporary.path();
    QFileInfoList result;
    QDir tDir(tmpDir);

    foreach(QFileInfo readDir, tDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden , QDir::Name)) {
        QString newFileName = readDir.fileName();
        if (QFileInfo(destination + "/" + newFileName).exists())
        {
            //if the generateUniqueDocumentPath is called twice in the same millisecond, the destination files are overwritten
            do
            {
                newFileName = QFileInfo(UBPersistenceManager::persistenceManager()->generateUniqueDocumentPath(tmpDir)).fileName();
            } while (QFileInfo(destination + "/" + newFileName).exists());
        }

        QString newFilePath = destination + "/" + newFileName;
        if (UBFileSystemUtils::copy(readDir.absoluteFilePath(), newFilePath, true)) {
            result.append(QFileInfo(newFilePath));
        }
    }

    return result;
}
