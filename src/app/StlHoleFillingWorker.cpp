#include "StlHoleFillingWorker.h"
#include "StlHoleFilling.h"

#include <vector>

namespace Mayo {

    StlHoleFillingWorker::StlHoleFillingWorker(QObject* parent)
        : QObject(parent)
    {
    }

    void StlHoleFillingWorker::process(const QString& inPath, const QString& outPath)
    {
        try {
            std::string infile = inPath.toStdString();
            std::string outfile = outPath.toStdString();

            std::vector<Triangles> triangles;

            bool isBin = isBinarySTL(infile);

            if (isBin)
                readBinarySTL(infile, triangles);
            else
                readASCIISTL(infile, triangles);

            if (triangles.empty()) {
                emit error("No triangles read from file.");
                return;
            }

            SurfaceMesh mesh = convertToSurfaceMesh(triangles);

            fillHolesCGAL(mesh);

            writeSTL(outfile, mesh);

            emit finished();
        }
        catch (std::exception& e) {
            emit error(QString::fromStdString(e.what()));
        }
    }

} // namespace Mayo
