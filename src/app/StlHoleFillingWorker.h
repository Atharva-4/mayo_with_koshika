#pragma once

#include <QObject>
#include <QString>

namespace Mayo {

    class StlHoleFillingWorker : public QObject
    {
        Q_OBJECT

    public:
        explicit StlHoleFillingWorker(QObject* parent = nullptr);

    public slots:
        void process(const QString& inPath, const QString& outPath);

    signals:
        void finished();
        void error(const QString& message);
    };

} // namespace Mayo
