// SPDX-License-Identifier: GPL-3.0-only
// Included in the native smoke-test translation unit's anonymous namespace.
bool checkPdfPageSwitch(QString *error)
{
    const QDir output = QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    QString fixture = qEnvironmentVariable("TEACHING_PDF_FIXTURE");
    if (qEnvironmentVariable("TEACHING_PDF_SYNTHETIC") == "1") {
        fixture = output.filePath("synthetic-navigation.pdf");
        QPdfWriter writer(fixture);
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QPageSize::Letter));
        writer.setPageMargins(QMarginsF());
        QPainter painter(&writer);
        for (int page = 1; page <= 3; ++page) {
            if (page > 1) writer.newPage();
            painter.fillRect(QRect(0,0,612,792), Qt::white);
            painter.setPen(Qt::black);
            painter.setFont(QFont("Arial",18));
            painter.drawText(QPoint(40,40),QString("GENERATED REGRESSION PAGE %1").arg(page));
            painter.setFont(QFont("Arial",12));
            for (int row = 0; row < 26; ++row) {
                const int y = 60 + row*26 + page*4;
                painter.drawText(QPoint(25+page*9,y),QString("%1.  y = %2 x^2 + %3 x - %4").arg(row+1).arg(page).arg(row+2).arg(page+row));
                painter.drawLine(350,y-4,560,y-4);
                painter.drawEllipse(QPoint(400+(row%5)*20,y-8),5+page,5+page);
            }
            // Image content as well as text, independent of any private quiz.
            QImage image(1200,900,QImage::Format_RGB32);
            for (int y=0;y<image.height();++y) for(int x=0;x<image.width();++x)
                image.setPixel(x,y,qRgb((x*page+y)%256,(y*page+x)%256,(x+y*2)%256));
            painter.drawImage(QRect(400,600,150,110),image);
        }
        painter.end();
    }
    auto board = UBApplication::boardController;
    auto view = board->controlView();
    auto window = UBApplication::mainWindow;
    auto fail = [&](const QString &message) { *error = message; return false; };
    if (!QFileInfo::exists(fixture)) return fail("PDF page-switch fixture missing");

    // Build independent references before importing. Never use an uncached
    // render to verify a live page: that could perturb the bug being tested.
    QVector<QImage> references;
    auto reference = PDFRenderer::rendererForUuid(QUuid::createUuid(), fixture, true);
    reference->attach();
    if (!reference->isValid() || reference->pageCount() != 3) {
        reference->detach(); return fail("Expected a readable three-page PDF");
    }
    reference->setDPI(144);
    for (int page = 1; page <= 3; ++page) {
        QImage image(reference->pageSizeF(page).toSize(), QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        reference->render(&painter, page, false);
        painter.end();
        references.append(image);
        image.save(output.filePath(QString("pdf-switch-reference-%1-window.png").arg(page)));
    }
    reference->detach();

    QJsonArray observations;
    auto record = [&] {
        QFile file(output.filePath("pdf-switch-observations.json"));
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(observations).toJson());
    };
    auto visible = [&](int page, const QString &tag, int timeoutMs) {
        QElapsedTimer timer; timer.start();
        int samples = 0, matches = 0, whiteSamples = 0, whiteMatches = 0;
        QImage screen;
        do {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            QThread::msleep(40);
            UBGraphicsPDFItem *pdf = nullptr;
            for (auto item : board->activeScene()->items())
                if (auto candidate = dynamic_cast<UBGraphicsPDFItem*>(item)) { pdf = candidate; break; }
            if (!pdf || pdf->pageNumber() != page) break;
            auto viewport = view->viewport();
            auto display = viewport->screen();
            const QPoint origin = viewport->mapToGlobal(QPoint());
            screen = display->grabWindow(0, origin.x(), origin.y(), viewport->width(), viewport->height()).toImage();
            if (screen.isNull()) break;
            screen = screen.scaled(viewport->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            samples = matches = whiteSamples = whiteMatches = 0;
            const auto bounds = pdf->boundingRect();
            const auto &expected = references[page - 1];
            // Compare actual on-screen ink pixels with this page's reference,
            // excluding UI edges, white paper and the added red test strokes.
            for (int y = 70; y < screen.height() - 140; y += 3)
                for (int x = 120; x < screen.width() - 120; x += 3) {
                    const auto p = pdf->mapFromScene(view->mapToScene(QPoint(x,y)));
                    if (!bounds.contains(p)) continue;
                    const int rx = qBound(0, int((p.x()-bounds.left())/bounds.width()*expected.width()), expected.width()-1);
                    const int ry = qBound(0, int((p.y()-bounds.top())/bounds.height()*expected.height()), expected.height()-1);
                    const QColor actual = screen.pixelColor(x,y);
                    if (qGray(expected.pixel(rx,ry)) > 245) {
                        ++whiteSamples;
                        if (qGray(actual.rgb()) > 200) ++whiteMatches;
                        continue;
                    }
                    if (qGray(expected.pixel(rx,ry)) > 100) continue;
                    if (actual.red() > actual.green() + 70) continue;
                    ++samples;
                    int darkest = 255;
                    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                        darkest = qMin(darkest,qGray(screen.pixel(x+dx,y+dy)));
                    if (darkest < 165) ++matches;
                }
            if (samples >= 50 && matches * 100 >= samples * 55 && whiteSamples >= 100 && whiteMatches*100 >= whiteSamples*80) break;
        } while (timer.elapsed() < timeoutMs);
        const bool good = samples >= 50 && matches * 100 >= samples * 55 && whiteSamples >= 100 && whiteMatches*100 >= whiteSamples*80;
        screen.save(output.filePath(tag + "-window.png"));
        observations.append(QJsonObject{{"tag",tag},{"page",page},{"elapsedMs",timer.elapsed()},
            {"referenceInkSamples",samples},{"matchingScreenSamples",matches},
            {"whiteSamples",whiteSamples},{"whiteMatches",whiteMatches},{"passed",good}});
        record();
        qWarning() << "PDF switch" << tag << "page" << page << "ms" << timer.elapsed() << "samples" << samples << "matches" << matches;
        return good;
    };
    auto stroke = [&] {
        window->actionPen->trigger();
        UBDrawingController::drawingController()->setPenColor(false, QColor(Qt::red), 0);
        const QPoint start(view->viewport()->width()/2-90, view->viewport()->height()/2);
        auto send = [&](QEvent::Type type, QPoint p, Qt::MouseButton button, Qt::MouseButtons buttons) {
            QMouseEvent event(type,QPointF(p),QPointF(view->viewport()->mapToGlobal(p)),button,buttons,Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &event);
        };
        send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        for(int dx=10;dx<=180;dx+=10) send(QEvent::MouseMove,start+QPoint(dx,dx/5),Qt::NoButton,Qt::LeftButton);
        send(QEvent::MouseButtonRelease,start+QPoint(180,36),Qt::LeftButton,Qt::NoButton);
    };

    // Exercise the same Documents import slot and file picker as the UI.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    for (int attempt = 0; attempt < 3; ++attempt) {
        UBApplication::applicationController->showDocument();
        bool selected = false;
        QTimer chooser;
        QElapsedTimer chooserElapsed; chooserElapsed.start();
        qWarning() << "PDF switch starting Documents import" << attempt;
        QObject::connect(&chooser, &QTimer::timeout, [&] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<QFileDialog*>(widget); dialog && dialog->isVisible()) {
                    if (chooserElapsed.elapsed() > 15000) { dialog->reject(); return; }
                    const QFileInfo target(fixture);
                    dialog->setDirectory(target.absolutePath());
                    dialog->selectFile(target.fileName());
                    if (auto edit = dialog->findChild<QLineEdit*>("fileNameEdit")) edit->setText(target.fileName());
                    selected = true;
                    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                }
        });
        chooser.start(50);
        UBApplication::documentController->importFile();
        chooser.stop();
        qWarning() << "PDF switch Documents import returned" << attempt;
        if (!selected) return fail("Documents import picker was not exercised");
        const auto doc = UBApplication::documentController->selectedDocumentProxy();
        if (!doc || doc->pageCount()!=3) return fail("Documents import did not create three pages");
        UBApplication::documentController->openSelectedItem();
        if (board->selectedDocument()!=doc) return fail("Opening the imported document did not activate it");
        board->setActiveDocumentScene(doc,0);
        board->zoom(1.0/board->currentZoom(),view->mapToScene(view->viewport()->rect().center()));
        const QString prefix = QString("pdf-switch-%1").arg(attempt);
        if (!visible(1,prefix+"-initial",15000)) return fail("Initial imported PDF page did not display");
        const auto countBefore = board->activeScene()->items().size();
        stroke();
        if (board->activeScene()->items().size() <= countBefore) return fail("Mouse drawing did not add ink");
        const qreal zoom = QList<qreal>{1.5,2.5,4.0}[attempt];
        board->zoom(zoom/board->currentZoom(),view->mapToScene(view->viewport()->rect().center()));
        if (qAbs(board->currentZoom()-zoom)>0.01) return fail("Requested zoom was not applied");
        // Dispatch the zoom paint now so its asynchronous PDF request exists
        // before navigation; do not wait for the rendering worker to finish.
        view->viewport()->repaint();
        const auto countBeforeZoomStroke = board->activeScene()->items().size();
        stroke();
        if (board->activeScene()->items().size() <= countBeforeZoomStroke) return fail("Drawing after zoom did not add ink");
        // No wait for the zoom renderer: navigate while its work is outstanding.
        for (int page : {2,3,1,2,3}) {
            while (board->activeSceneIndex() < page-1) board->nextScene();
            while (board->activeSceneIndex() > page-1) board->previousScene();
            if (!visible(page,prefix+QString("-page-%1").arg(page),15000))
                return fail(QString("PDF page %1 stayed blank or differed after draw/zoom (attempt %2)").arg(page).arg(attempt));
        }
        board->persistCurrentScene(false,true);
        UBPersistenceManager::persistenceManager()->flushPendingWrites();
        board->setActiveDocumentScene(doc,1,true);
        if (!visible(2,prefix+"-reload-page-2",15000)) return fail("PDF page 2 failed after save/reload");
    }
    return true;
}
