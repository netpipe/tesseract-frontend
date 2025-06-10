#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include <QMenuBar>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QProcess>
#include <QDropEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QDir>
#include <qdebug.h>
class ImageLabel : public QLabel {
    Q_OBJECT
public:
    explicit ImageLabel(QWidget *parent = nullptr) : QLabel(parent) {
        setAcceptDrops(true);
        setAlignment(Qt::AlignCenter);
    }

    QRect selectionRect;
    QPoint startPos;
    bool selecting = false;

signals:
    void imageDropped(const QString &path);
    void regionSelected(const QRect &rect);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override {
        auto urls = event->mimeData()->urls();
        if (!urls.isEmpty())
            emit imageDropped(urls.first().toLocalFile());
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (!pixmap()) return;
        startPos = event->pos();
        selecting = true;
        selectionRect = QRect();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (selecting) {
            selectionRect = QRect(startPos, event->pos()).normalized();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (selecting) {
            selecting = false;
            update();
            emit regionSelected(selectionRect);
        }
    }

    void paintEvent(QPaintEvent *event) override {
        QLabel::paintEvent(event);
        if (!selectionRect.isNull()) {
            QPainter p(this);
            p.setPen(QPen(Qt::red, 2));
            p.drawRect(selectionRect);
        }
    }
};

class OCRWindow : public QMainWindow {
    Q_OBJECT
public:
    OCRWindow() {
        setWindowTitle("Tesseract OCR GUI");
        resize(1000, 600);

        imageLabel = new ImageLabel;
        textEdit = new QTextEdit;
        textEdit->setReadOnly(true);

        auto *layout = new QHBoxLayout;
        layout->addWidget(imageLabel, 1);
        layout->addWidget(textEdit, 1);

        auto *central = new QWidget;
        central->setLayout(layout);
        setCentralWidget(central);

        setupMenu();

        connect(imageLabel, &ImageLabel::imageDropped, this, &OCRWindow::loadImage);
        connect(imageLabel, &ImageLabel::regionSelected, this, &OCRWindow::performOCROnRegion);
    }

private:
    ImageLabel *imageLabel;
    QTextEdit *textEdit;
    QString currentImagePath;
    QString lang = "eng";

    void setupMenu() {
        auto *langMenu = menuBar()->addMenu("Language");
        auto addLangAction = [&](const QString &code, const QString &label) {
            QAction *act = langMenu->addAction(label);
            connect(act, &QAction::triggered, this, [=]() { lang = code; });
        };

        addLangAction("eng", "English");
        addLangAction("spa", "Spanish");
        addLangAction("deu", "German");

        auto *fileMenu = menuBar()->addMenu("File");
        fileMenu->addAction("Batch Folder", this, &OCRWindow::batchProcessFolder);
    }

    void loadImage(const QString &path) {
        QPixmap img(path);
        if (img.isNull()) return;
        imageLabel->setPixmap(img);
        currentImagePath = path;
        performOCR(path);
    }

    void performOCR(const QString &path, const QRect &rect = QRect()) {
        QString command = "/tesseract";
        QStringList args;

        QString croppedImage = path;

        if (!rect.isNull()) {
            QImage original(path);
            if (original.isNull()) return;

          //  double scaleX = static_cast<double>(original.width()) / imageLabel->width();
          //  double scaleY = static_cast<double>(original.height()) / imageLabel->height();
            QRect scaledRect(rect.x(), rect.y(), rect.width(), rect.height());
            QImage cropped = original.copy(scaledRect);
          //  croppedImage = QDir::tempPath() + "/ocr_crop.png";
            croppedImage = QApplication::applicationDirPath() + "/ocr_crop.png";
            cropped.save(croppedImage);
        //    qDebug() << "testing";
        }

        args << croppedImage << "-" << "-l" << lang;

        QProcess proc;
        proc.start(QApplication::applicationDirPath() + command, args);
        proc.waitForFinished();

        QString result = proc.readAllStandardOutput();
        textEdit->setPlainText(result);
       //  qDebug() << result;
    }

    void performOCROnRegion(const QRect &rect) {
        if (!currentImagePath.isEmpty())
            performOCR(currentImagePath, rect);
    }

    void batchProcessFolder() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Folder");
        if (dir.isEmpty()) return;

        QDir directory(dir);
        QStringList images = directory.entryList(QStringList() << "*.png" << "*.jpg", QDir::Files);

        for (const QString &file : images) {
            QString input = directory.filePath(file);
            QString baseName = QFileInfo(file).completeBaseName();
            QString output = directory.filePath(baseName + ".txt");

            QProcess::execute("tesseract", QStringList() << input << output << "-l" << lang);
        }
    }
};



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    OCRWindow win;
    win.show();
    return app.exec();
}
#include "main.moc"
