#include "StlCuttingWorker.h"

#include "STLCutter.h"

namespace Mayo {

StlCuttingWorker::StlCuttingWorker(QObject* parent)
    : QObject(parent)
{
}

void StlCuttingWorker::process(const QString& inPath, const QString& outAbove, const QString& outBelow, char axis, float cutValue)
{
    try {
        STLCutter cutter;
        const std::vector<Facet> facets = cutter.loadSTL(inPath.toStdString());
        if (facets.empty()) {
            emit error(QStringLiteral("Unable to load STL facets."));
            return;
        }

        std::vector<Facet> above;
        std::vector<Facet> below;
        cutter.cutMesh(facets, axis, cutValue, above, below);
        cutter.saveSTL(outAbove.toStdString(), above);
        cutter.saveSTL(outBelow.toStdString(), below);

        emit finished();
    }
    catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    }
}

} // namespace Mayo