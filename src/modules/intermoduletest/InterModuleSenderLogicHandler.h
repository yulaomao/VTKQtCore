#pragma once

#include "logic/registry/ModuleLogicHandler.h"

class InterModuleSenderLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit InterModuleSenderLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;

private:
    void emitShellError(const QString& errorCode,
                        const QString& message,
                        const QString& sourceActionId);
};