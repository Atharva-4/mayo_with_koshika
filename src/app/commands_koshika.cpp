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

#include "STLMerger.h"// Stl mergeing

#include "StlCuttingWorker.h" // worker class for cutting
#include <QtCore/QSignalBlocker>

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Graphic3d_ClipPlane.hxx>

#include "StlHoleFillingSelectedWorker.h"
#include <QInputDialog>
#include <QLineEdit>

#include <algorithm>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>

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
        if (m_isRunning)
            return;

        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc)
            return;

        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty())
            return;

        const QString inputPath = QString::fromStdString(filePath.u8string());

        STLCutter cutter;
        std::vector<Facet> facets = cutter.loadSTL(inputPath.toStdString());
        if (facets.empty()) {
            QMessageBox::warning(this->widgetMain(), tr("Split into two"), tr("Unable to load STL facets."));
            return;
        }

        // ---------------------------
        // 1. Plane selection + position dialog
        // ---------------------------
        QDialog dlg(this->widgetMain());
        dlg.setWindowTitle(tr("Splitting Plane"));

        auto comboPlane = new QComboBox(&dlg);
        comboPlane->addItem("YZ Plane");
        comboPlane->addItem("XZ Plane");
        comboPlane->addItem("XY Plane");

        auto sliderCutPos = new QSlider(Qt::Horizontal, &dlg);
        sliderCutPos->setRange(0, 1000);
        auto spinCutPos = new QDoubleSpinBox(&dlg);
        spinCutPos->setDecimals(4);
        auto checkPreview = new QCheckBox(tr("Preview in viewport while moving slider"), &dlg);
        checkPreview->setChecked(true);

        auto clipPlanePreview = new Graphic3d_ClipPlane(gp_Pln(gp::Origin(), gp::DX()));
        clipPlanePreview->SetOn(false);
        clipPlanePreview->SetCapping(false); // only clip, don't show capping plane
        guiDoc->v3dView()->AddClipPlane(clipPlanePreview);

        auto setPreviewPlanePosition = [=](double pos) {
            const gp_Dir normal = clipPlanePreview->ToPlane().Axis().Direction();
            const gp_XYZ xyz = gp_Vec(normal).XYZ() * pos;
            clipPlanePreview->SetEquation(gp_Pln(gp_Pnt(xyz), normal));
            };

        auto updateAxisAndRange = [&]() {
            char axis = 'X';
            gp_Dir normal = gp::DX();
            if (comboPlane->currentText() == "XZ Plane") {
                axis = 'Y';
                normal = gp::DY();
            }
            else if (comboPlane->currentText() == "XY Plane") {
                axis = 'Z';
                normal = gp::DZ();
            }

            float minV = 0.f;
            float maxV = 0.f;
            cutter.getBounds(facets, axis, minV, maxV);
            const double span = maxV - minV;
            const double gap = span * 0.01;
            spinCutPos->setRange(minV - gap, maxV + gap);
            spinCutPos->setSingleStep(std::abs(maxV - minV) / 200.0);
            spinCutPos->setValue((minV + maxV) * 0.5);
            clipPlanePreview->SetEquation(gp_Pln(gp::Origin(), normal));
            setPreviewPlanePosition(spinCutPos->value());
            };

        QObject::connect(comboPlane, &QComboBox::currentTextChanged, &dlg, [=](const QString&) {
            updateAxisAndRange();
            });

        QObject::connect(spinCutPos, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, [=](double value) {
            const double ratio = (value - spinCutPos->minimum()) /
                std::max(1e-9, spinCutPos->maximum() - spinCutPos->minimum());
            {
                QSignalBlocker blocker(sliderCutPos);
                sliderCutPos->setValue(qRound(ratio * sliderCutPos->maximum()));
            }

            if (checkPreview->isChecked()) {
                clipPlanePreview->SetOn(true);
                setPreviewPlanePosition(value);
                guiDoc->graphicsView().redraw();
            }
            });

        QObject::connect(sliderCutPos, &QSlider::valueChanged, &dlg, [=](int value) {
            const double ratio = double(value) / std::max(1, sliderCutPos->maximum());
            const double cutPos = spinCutPos->minimum() + ratio * (spinCutPos->maximum() - spinCutPos->minimum());
            QSignalBlocker blocker(spinCutPos);
            spinCutPos->setValue(cutPos);

            if (checkPreview->isChecked()) {
                clipPlanePreview->SetOn(true);
                setPreviewPlanePosition(cutPos);
                guiDoc->graphicsView().redraw();
            }
            });

        QObject::connect(checkPreview, &QCheckBox::toggled, &dlg, [=](bool on) {
            clipPlanePreview->SetOn(on);
            guiDoc->graphicsView().redraw();
            });

        auto btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &dlg
        );

        auto layoutCutPos = new QHBoxLayout;
        layoutCutPos->addWidget(new QLabel(tr("Cut value"), &dlg));
        layoutCutPos->addWidget(spinCutPos);

        auto layout = new QVBoxLayout(&dlg);
        layout->addWidget(new QLabel(tr("Choose plane orientation"), &dlg));
        layout->addWidget(comboPlane);
        layout->addWidget(sliderCutPos);
        layout->addLayout(layoutCutPos);
        layout->addWidget(checkPreview);
        layout->addWidget(btnBox);

        QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        updateAxisAndRange();

        if (dlg.exec() != QDialog::Accepted) {
            guiDoc->v3dView()->RemoveClipPlane(clipPlanePreview);
            guiDoc->graphicsView().redraw();
            return;
        }

        // ---------------------------
        // 2. Plane enum -> axis
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

        const float cutValue = static_cast<float>(spinCutPos->value());

        guiDoc->v3dView()->RemoveClipPlane(clipPlanePreview);
        guiDoc->graphicsView().redraw();

        // ------------------------------------------
        // 3. Ask user for save directory
        // ------------------------------------------
        QString dirPath = QFileDialog::getExistingDirectory(
            this->widgetMain(),
            tr("Select Folder to Save Cut STL Files")
        );

        if (dirPath.isEmpty())
            return;

        // Build output file paths
        const QString outAbove = dirPath + "/cut_1.stl";
        const QString outBelow = dirPath + "/cut_2.stl";

        m_isRunning = true;

        QThread* thread = new QThread(this);
        auto* worker = new StlCuttingWorker();
        worker->moveToThread(thread);

        connect(thread, &QThread::started, this, [=]() {
            QMetaObject::invokeMethod(worker, "process",
                Qt::QueuedConnection,
                Q_ARG(QString, inputPath),
                Q_ARG(QString, outAbove),
                Q_ARG(QString, outBelow),
                Q_ARG(char, axis),
                Q_ARG(float, cutValue));
            });

        connect(worker, &StlCuttingWorker::finished, this, [=]() {
            QMessageBox::information(
                this->widgetMain(),
                tr("Split into two"),
                tr("Cut STL files written to:\n%1\n%2").arg(outAbove, outBelow)
            );
            m_isRunning = false;
            thread->quit();
            });

        connect(worker, &StlCuttingWorker::error, this, [=](const QString& msg) {
            QMessageBox::critical(this->widgetMain(), tr("Split Error"), msg);
            m_isRunning = false;
            thread->quit();
            });

        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
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
        if (m_isRunning)
            return;

        m_isRunning = true;

        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc) {
            m_isRunning = false;
            return;
        }

        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty()) {
            m_isRunning = false;
            return;
        }

        QString inPath = QString::fromStdString(filePath.u8string());

        QWidget* parent = widgetMain();

        QString outPath = QFileDialog::getSaveFileName(
            parent,
            tr("Save repaired STL"),
            QFileInfo(inPath).completeBaseName() + "_repaired.stl",
            tr("STL Files (*.stl);;All Files (*)"));

        if (outPath.isEmpty()) {
            m_isRunning = false;
            return;
        }

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
                m_isRunning = false;  // reset here
                thread->quit();
            });

        connect(worker, &Mayo::StlHoleFillingWorker::error,
            this, [=](const QString& msg) {
                QMessageBox::critical(parent,
                    tr("Hole Filling Error"),
                    msg);
                m_isRunning = false;  // reset here
                thread->quit();
            });

        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }


    /////////////////////////////////////////////////////////////////////
    //
    //Hole Filling With Selection
    // 
    ////////////////////////////////////////////////////////////////////


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
        if (m_isRunning)
            return;

        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc)
            return;

        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty())
            return;

        const QString inPath = QString::fromStdString(filePath.u8string());
        QWidget* parent = widgetMain();

        bool ok = false;
        const QString txtIds = QInputDialog::getText(
            parent,
            tr("Fill Holes (Selected)"),
            tr("Enter hole indices (comma separated, e.g. 0,2,5):"),
            QLineEdit::Normal,
            QString(),
            &ok
        );
        if (!ok || txtIds.trimmed().isEmpty())
            return;

        QVector<int> selectedIds;
        for (const QString& part : txtIds.split(',', Qt::SkipEmptyParts)) {
            bool idOk = false;
            const int id = part.trimmed().toInt(&idOk);
            if (idOk)
                selectedIds.push_back(id);
        }

        if (selectedIds.isEmpty()) {
            QMessageBox::warning(parent, tr("Fill Holes (Selected)"), tr("No valid hole index entered."));
            return;
        }

        const QString outPath = QFileDialog::getSaveFileName(
            parent,
            tr("Save repaired STL"),
            QFileInfo(inPath).completeBaseName() + "_selected_repaired.stl",
            tr("STL Files (*.stl);;All Files (*)")
        );
        if (outPath.isEmpty())
            return;

        QThread* thread = new QThread(this);
        auto* worker = new StlHoleFillingSelectedWorker();
        worker->moveToThread(thread);
        m_isRunning = true;

        connect(thread, &QThread::started, this, [=]() {
            QMetaObject::invokeMethod(worker, "process",
                Qt::QueuedConnection,
                Q_ARG(QString, inPath),
                Q_ARG(QString, outPath),
                Q_ARG(QVector<int>, selectedIds));
            });

        connect(worker, &StlHoleFillingSelectedWorker::finished, this, [=]() {
            QMessageBox::information(parent, tr("Fill Holes (Selected)"),
                tr("Repaired STL written to:\n%1").arg(outPath));
            m_isRunning = false;
            thread->quit();
            });

        connect(worker, &StlHoleFillingSelectedWorker::error, this, [=](const QString& msg) {
            QMessageBox::critical(parent, tr("Fill Holes (Selected) Error"), msg);
            m_isRunning = false;
            thread->quit();
            });

        connect(thread, &QThread::finished, this, [this]() {
            m_isRunning = false; // extra safety
            });
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }


    


 ///////////////////////////////////////////////////////////////////
//
// STL Merge Command
//
///////////////////////////////////////////////////////////////////

    CommandMergeSTL::CommandMergeSTL(IAppContext* context)
        : Command(context)
    {
        auto action = new QAction(this);
        action->setText(Command::tr("Merge STL Files"));
        action->setToolTip(Command::tr("Merge multiple STL files into a single mesh"));
        this->setAction(action);
    }

    void CommandMergeSTL::execute()
    {
        QWidget* parent = this->widgetMain();

        const QStringList inPaths = QFileDialog::getOpenFileNames(
            parent,
            tr("Select STL files to merge"),
            QString(),
            tr("STL Files (*.stl);;All Files (*)")
        );

        if (inPaths.size() < 2) {
            if (!inPaths.isEmpty()) {
                QMessageBox::information(parent, tr("Merge STL Files"), tr("Select at least two STL files."));
            }
            return;
        }

        const QString outPath = QFileDialog::getSaveFileName(
            parent,
            tr("Save merged STL"),
            "merged.stl",
            tr("STL Files (*.stl);;All Files (*)")
        );

        if (outPath.isEmpty())
            return;

        STLCutter cutter;
        std::vector<Facet> mergedFacets;

        for (const QString& inPath : inPaths) {
            const auto facets = cutter.loadSTL(inPath.toStdString());
            if (facets.empty()) {
                QMessageBox::warning(
                    parent,
                    tr("Merge STL Files"),
                    tr("Failed to load or empty STL:\n%1").arg(inPath)
                );
                return;
            }

            mergedFacets.insert(mergedFacets.end(), facets.begin(), facets.end());
        }

        cutter.saveSTL(outPath.toStdString(), mergedFacets);
        QMessageBox::information(
            parent,
            tr("Merge STL Files"),
            tr("Merged STL written to:\n%1").arg(outPath)
        );
    }




} // namespace Mayo
