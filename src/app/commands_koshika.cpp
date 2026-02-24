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

#include <QtCore/QSignalBlocker>

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Graphic3d_ClipPlane.hxx>

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
        GuiDocument* guiDoc = this->currentGuiDocument();
        if (!guiDoc)
            return;

        const FilePath filePath = guiDoc->document()->filePath();
        if (filePath.empty())
            return;

        const std::string inputFile = filePath.u8string();

        STLCutter cutter;
        std::vector<Facet> facets = cutter.loadSTL(inputFile);
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

        std::vector<Facet> above, below;
        cutter.cutMesh(facets, axis, cutValue, above, below);

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
        // Placeholder: We'll implement selection-aware filling next.
        QWidget* parent = widgetMain();
        QMessageBox::information(parent, tr("Hole Filling"), tr("Selected hole filling is not implemented yet."));
    }


    
    ///////////////////////////////////////////////////////////////////////////
    //
	//Merging Stl Files
    //
    //////////////////////////////////////////////////////////////////////////


 ///////////////////////////////////////////////////////////////////
//
// STL Merge Command
//
///////////////////////////////////////////////////////////////////

    CommandMergeSTL::CommandMergeSTL(IAppContext* context)
        : Command(context)
    {
        auto action = new QAction(this);
        action->setText(tr("Merge STL Files"));
        action->setToolTip(tr("Merge two STL files into one"));
        setAction(action);

        connect(action, &QAction::triggered,
            this, &CommandMergeSTL::execute);
    }

    void CommandMergeSTL::execute()
    {
        QWidget* parent = widgetMain();

        // 1️⃣ Select first STL
        QString file1 = QFileDialog::getOpenFileName(
            parent,
            tr("Select First STL"),
            "",
            tr("STL Files (*.stl)")
        );

        if (file1.isEmpty())
            return;

        // 2️⃣ Select second STL
        QString file2 = QFileDialog::getOpenFileName(
            parent,
            tr("Select Second STL"),
            "",
            tr("STL Files (*.stl)")
        );

        if (file2.isEmpty())
            return;

        // 3️⃣ Select output file
        QString output = QFileDialog::getSaveFileName(
            parent,
            tr("Save Merged STL"),
            "merged.stl",
            tr("STL Files (*.stl)")
        );

        if (output.isEmpty())
            return;

        // 4️⃣ Run merger
        STLMerger merger;

        if (!merger.loadSTL(file1.toStdString())) {
            QMessageBox::critical(parent, tr("Error"),
                tr("Failed to load first STL"));
            return;
        }

        if (!merger.loadSTL(file2.toStdString())) {
            QMessageBox::critical(parent, tr("Error"),
                tr("Failed to load second STL"));
            return;
        }

        merger.merge();

        if (!merger.saveMerged(output.toStdString())) {
            QMessageBox::critical(parent, tr("Error"),
                tr("Failed to save merged STL"));
            return;
        }

        QMessageBox::information(parent,
            tr("Merge Complete"),
            tr("Merged STL saved successfully."));
    }




} // namespace Mayo
