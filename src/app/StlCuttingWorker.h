#pragma once

#include <QObject>
#include <QString>

namespace Mayo {

class StlCuttingWorker : public QObject {
    Q_OBJECT
public:
    explicit StlCuttingWorker(QObject* parent = nullptr);

public slots:
    void process(const QString& inPath, const QString& outAbove, const QString& outBelow, char axis, float cutValue);

signals:
    void finished();
    void error(const QString& message);
};

} // namespace Mayo