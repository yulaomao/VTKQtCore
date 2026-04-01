#include "ParamsPage.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

ParamsPage::ParamsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral("Parameters Configuration"), this);
    mainLayout->addWidget(titleLabel);

    auto* formLayout = new QFormLayout;

    m_patientNameEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Patient Name"), m_patientNameEdit);

    m_studyIdEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Study ID"), m_studyIdEdit);

    m_procedureTypeEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Procedure Type"), m_procedureTypeEdit);

    mainLayout->addLayout(formLayout);

    m_applyButton = new QPushButton(QStringLiteral("Apply Parameters"), this);
    mainLayout->addWidget(m_applyButton);

    mainLayout->addStretch();

    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        QVariantMap params;
        params.insert(QStringLiteral("patientName"), m_patientNameEdit->text());
        params.insert(QStringLiteral("studyId"), m_studyIdEdit->text());
        params.insert(QStringLiteral("procedureType"), m_procedureTypeEdit->text());
        emit parameterApplied(params);
    });
}
