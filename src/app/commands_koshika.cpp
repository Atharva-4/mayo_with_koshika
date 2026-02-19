#include "commands_koshika.h"

#include "../gui/gui_document.h"
#include "../gui/v3d_view_controller.h"
#include <QAction>
#include <QtWidgets/QDialog>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>

#include <QFileInfo> // tostdstring
#include "STLCutter.h"   // your algorithm
#include "StlHoleFilling.h" // your header in Mayo namespace
#include <QMessageBox>

#include <QThread>// holeFilling using Thread
#include "StlHoleFillingWorker.h"// worker class for hole filling

namespace Mayo {

    CommandCutting::CommandCutting(IAppContext* context)
        : Command(context)
    {
        auto action = new QAction(this);
        action->setText(Command::tr("Split into two"));
        action->setToolTip(Command::tr("Split the current model into two parts using a plane"));
        this->setAction(action);
    }
    void CommandCutting::execute()
    {
        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc)
            return;

        // ---------------------------
        // 1. Plane selection dialog
        // ---------------------------
        QDialog dlg(this->widgetMain());
        dlg.setWindowTitle(tr("Spliting Plane"));

        auto comboPlane = new QComboBox(&dlg);
        comboPlane->addItem("YZ Plane");
        comboPlane->addItem("XZ Plane");
        comboPlane->addItem("XY Plane");

        auto btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &dlg
        );

        auto layout = new QVBoxLayout(&dlg);
        layout->addWidget(comboPlane);
        layout->addWidget(btnBox);

        QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted)
            return;

        // ---------------------------
        // 2. Plane enum → axis
        // ---------------------------
        CommandCutting::CutPlane plane = CommandCutting::CutPlane::Z;

        const QString selected = comboPlane->currentText();
        if (selected == "YZ Plane") plane = CommandCutting::CutPlane::X;
        else if (selected == "XZ Plane") plane = CommandCutting::CutPlane::Y;
        else if (selected == "XY Plane") plane = CommandCutting::CutPlane::Z;

        char axis = 'Z';
        switch (plane) {
        case CommandCutting::CutPlane::X: axis = 'X'; break;
        case CommandCutting::CutPlane::Y: axis = 'Y'; break;
        case CommandCutting::CutPlane::Z: axis = 'Z'; break;
        }

        // ---------------------------
        // 3. Get STL file path
        // ---------------------------
        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty())
            return;

        const std::string inputFile = filePath.u8string();

        // ---------------------------
        // 4. Run STL cutting
        // ---------------------------
        STLCutter cutter;

        std::vector<Facet> facets = cutter.loadSTL(inputFile);

        float minV = 0.f, maxV = 0.f;
        cutter.getBounds(facets, axis, minV, maxV);
        float cutValue = (minV + maxV) * 0.5f;

        std::vector<Facet> above, below;
        cutter.cutMesh(facets, axis, cutValue, above, below);

        // ------------------------------------------
        // 5. Ask user for save directory
        // ------------------------------------------
        QString dirPath = QFileDialog::getExistingDirectory(
            this->widgetMain(),
            tr("Select Folder to Save Cut STL Files")
        );

        if (dirPath.isEmpty())
            return;

        // Build output file paths
        std::string outAbove =
            (dirPath + "/cut_1.stl").toStdString();
        std::string outBelow =
            (dirPath + "/cut_2.stl").toStdString();

        // Save results
        cutter.saveSTL(outAbove, above);
        cutter.saveSTL(outBelow, below);


    }

    /////////////////////////////////////////////////////////////////////
    //
    //Hole Filling Without Selection
    // 
    ////////////////////////////////////////////////////////////////////

    CommandHoleFillingFull::CommandHoleFillingFull(IAppContext* context)
        : Command(context)
    {
        auto action = new QAction(this);
        action->setText(tr("Fill Holes (All)"));
        action->setToolTip(tr("Fill all holes in an STL mesh (full fill)"));
        setAction(action);
        connect(action, &QAction::triggered, this, &CommandHoleFillingFull::execute);
    }

    void CommandHoleFillingFull::execute()
    {
        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc)
            return;

        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty())
            return;

        QString inPath = QString::fromStdString(filePath.u8string());

        QWidget* parent = widgetMain();

        QString outPath = QFileDialog::getSaveFileName(
            parent,
            tr("Save repaired STL"),
            QFileInfo(inPath).completeBaseName() + "_repaired.stl",
            tr("STL Files (*.stl);;All Files (*)"));

        if (outPath.isEmpty())
            return;

        // Thread + Worker
        QThread* thread = new QThread(this);
        Mayo::StlHoleFillingWorker* worker = new Mayo::StlHoleFillingWorker();

        worker->moveToThread(thread);

        connect(thread, &QThread::started, this, [=]() {
            QMetaObject::invokeMethod(worker, "process",
                Qt::QueuedConnection,
                Q_ARG(QString, inPath),
                Q_ARG(QString, outPath));
            });

        connect(worker, &Mayo::StlHoleFillingWorker::finished,
            this, [=]() {
                QMessageBox::information(parent,
                    tr("Hole Filling"),
                    tr("Repaired STL written to:\n%1").arg(outPath));
                thread->quit();
            });

        connect(worker, &Mayo::StlHoleFillingWorker::error,
            this, [=](const QString& msg) {
                QMessageBox::critical(parent,
                    tr("Hole Filling Error"),
                    msg);
                thread->quit();
            });

        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }



    CommandHoleFillingSelected::CommandHoleFillingSelected(IAppContext* context)
        : Command(context)
    {
        auto action = new QAction(this);
        action->setText(tr("Fill Holes (Selected)"));
        action->setToolTip(tr("Fill only selected holes (not implemented yet)"));
        setAction(action);
        connect(action, &QAction::triggered, this, &CommandHoleFillingSelected::execute);
    }

    void CommandHoleFillingSelected::execute()
    {
        // Placeholder: We'll implement selection-aware filling next.
        QWidget* parent = widgetMain();
        QMessageBox::information(parent, tr("Hole Filling"), tr("Selected hole filling is not implemented yet."));
    }

} // namespace Mayo
