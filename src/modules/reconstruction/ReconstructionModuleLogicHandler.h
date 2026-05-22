#pragma once

#include "logic/registry/ModuleLogicHandler.h"

#include <QVariantMap>

class ReconstructionModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit ReconstructionModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handleStateSample(const StateSample& sample) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    void emitStatus(const QString& status, bool reconstructionDone = false);

    QString m_statusText = QStringLiteral("重建模块已就绪。");
    bool m_isReconstructionDone = false;
};
