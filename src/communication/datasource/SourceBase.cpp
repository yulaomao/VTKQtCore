#include "SourceBase.h"

SourceBase::SourceBase(const QString& sourceId, QObject* parent)
    : QObject(parent)
    , m_sourceId(sourceId)
{
}

QString SourceBase::getSourceId() const
{
    return m_sourceId;
}
