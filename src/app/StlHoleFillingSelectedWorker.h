#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Mayo {

class StlHoleFillingSelectedWorker : public QObject
{
    Q_OBJECT

public:
    explicit StlHoleFillingSelectedWorker(QObject* parent = nullptr);

public slots:
    void process(const QString& inPath, const QString& outPath, const QVector<int>& selectedIds);

signals:
    void finished();
    void error(const QString& message);
};

} // namespace Mayo