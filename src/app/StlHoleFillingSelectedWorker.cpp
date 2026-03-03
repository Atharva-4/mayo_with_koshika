#include "StlHoleFillingSelectedWorker.h"

#include "StlHoleFilling.h"

#include <set>
#include <vector>

namespace Mayo {

StlHoleFillingSelectedWorker::StlHoleFillingSelectedWorker(QObject* parent)
    : QObject(parent)
{
}

void StlHoleFillingSelectedWorker::process(const QString& inPath, const QString& outPath, const QVector<int>& selectedIds)
{
    try {
        std::string infile = inPath.toStdString();
        std::string outfile = outPath.toStdString();

        std::vector<Triangles> triangles;
        const bool isBin = isBinarySTL(infile);

        if (isBin)
            readBinarySTL(infile, triangles);
        else
            readASCIISTL(infile, triangles);

        if (triangles.empty()) {
            emit error(QStringLiteral("No triangles read from file."));
            return;
        }

        std::vector<int> selected;
        selected.reserve(selectedIds.size());

        std::set<int> uniq;
        for (int id : selectedIds) {
            if (id >= 0)
                uniq.insert(id);
        }
        selected.assign(uniq.begin(), uniq.end());

        if (selected.empty()) {
            emit error(QStringLiteral("No valid selected hole IDs."));
            return;
        }

        SurfaceMesh mesh = convertToSurfaceMesh(triangles);
        fillSelectedHolesCGAL(mesh, selected);
        writeSTL(outfile, mesh);

        emit finished();
    }
    catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    }
}

} // namespace Mayo