#pragma once
#include <QObject>
#include <QList>
#include "machine.h"

class MachineProvider : public QObject {
    Q_OBJECT
public:
    explicit MachineProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MachineProvider() = default;

public slots:
    virtual void fetchMachines() = 0;

signals:
    void machinesReady(const QList<Machine>& machines);
    void errorOccurred(const QString& message);
};