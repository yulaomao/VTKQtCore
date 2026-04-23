#include "DataGenPage.h"

#include "ui/coordination/UiActionDispatcher.h"
#include "ui/vtk3d/VtkSceneWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

void replaceLayoutWidget(QVBoxLayout* layout, QWidget* newWidget)
{
    if (!layout) {
        return;
    }

    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }

    if (newWidget) {
        newWidget->setParent(layout->parentWidget());
        layout->addWidget(newWidget);
    }
}

QDoubleSpinBox* createAxisSpinBox(QWidget* parent, double minimum, double maximum, double value)
{
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setValue(value);
    spinBox->setSingleStep(1.0);
    return spinBox;
}

QString nodeTypeLabel(const QString& nodeType)
{
    if (nodeType == QStringLiteral("point")) {
        return QStringLiteral("PointNode");
    }
    if (nodeType == QStringLiteral("line")) {
        return QStringLiteral("LineNode");
    }
    if (nodeType == QStringLiteral("model")) {
        return QStringLiteral("ModelNode");
    }
    if (nodeType == QStringLiteral("transform")) {
        return QStringLiteral("TransformNode");
    }
    return nodeType;
}

void setFormRowVisible(QFormLayout* form, QWidget* field, bool visible)
{
    if (!form || !field) {
        return;
    }

    if (QWidget* label = form->labelForField(field)) {
        label->setVisible(visible);
    }
    field->setVisible(visible);
}

}

DataGenPage::DataGenPage(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    auto* controlScrollArea = new QScrollArea(this);
    controlScrollArea->setWidgetResizable(true);
    controlScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlScrollArea->setMinimumWidth(440);
    controlScrollArea->setFrameShape(QFrame::NoFrame);

    auto* controlPanel = new QWidget(controlScrollArea);
    controlPanel->setMinimumWidth(420);
    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(10);

    auto* titleLabel = new QLabel(QStringLiteral("Data Generator"), controlPanel);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    controlLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("已就绪。可创建节点、编辑显示属性和父子变换。"), controlPanel);
    m_statusLabel->setWordWrap(true);
    controlLayout->addWidget(m_statusLabel);

    auto* createGroup = new QGroupBox(QStringLiteral("创建节点"), controlPanel);
    auto* createLayout = new QVBoxLayout(createGroup);
    auto* createForm = new QFormLayout();
    m_createTypeCombo = new QComboBox(createGroup);
    m_createTypeCombo->addItem(QStringLiteral("PointNode"), QStringLiteral("point"));
    m_createTypeCombo->addItem(QStringLiteral("LineNode"), QStringLiteral("line"));
    m_createTypeCombo->addItem(QStringLiteral("ModelNode"), QStringLiteral("model"));
    m_createTypeCombo->addItem(QStringLiteral("TransformNode"), QStringLiteral("transform"));
    m_createNameEdit = new QLineEdit(QStringLiteral("Generated Node"), createGroup);
    createForm->addRow(QStringLiteral("类型"), m_createTypeCombo);
    createForm->addRow(QStringLiteral("名称"), m_createNameEdit);
    createLayout->addLayout(createForm);

    m_createStack = new QStackedWidget(createGroup);

    auto* pointPage = new QWidget(m_createStack);
    auto* pointForm = new QFormLayout(pointPage);
    m_initialCountSpin = new QSpinBox(pointPage);
    m_initialCountSpin->setRange(1, 64);
    m_initialCountSpin->setValue(5);
    m_spacingSpin = createAxisSpinBox(pointPage, 1.0, 200.0, 16.0);
    pointForm->addRow(QStringLiteral("初始点数"), m_initialCountSpin);
    pointForm->addRow(QStringLiteral("点间距"), m_spacingSpin);
    m_createStack->addWidget(pointPage);

    auto* linePage = new QWidget(m_createStack);
    auto* lineForm = new QFormLayout(linePage);
    auto* lineCountSpin = new QSpinBox(linePage);
    lineCountSpin->setObjectName(QStringLiteral("lineCountSpin"));
    lineCountSpin->setRange(2, 64);
    lineCountSpin->setValue(4);
    auto* lineSpacingSpin = createAxisSpinBox(linePage, 1.0, 200.0, 24.0);
    lineSpacingSpin->setObjectName(QStringLiteral("lineSpacingSpin"));
    m_closedLineCheck = new QCheckBox(QStringLiteral("闭合折线"), linePage);
    lineForm->addRow(QStringLiteral("初始顶点数"), lineCountSpin);
    lineForm->addRow(QStringLiteral("顶点间距"), lineSpacingSpin);
    lineForm->addRow(QString(), m_closedLineCheck);
    m_createStack->addWidget(linePage);

    auto* modelPage = new QWidget(m_createStack);
    auto* modelForm = new QFormLayout(modelPage);
    m_modelShapeCombo = new QComboBox(modelPage);
    m_modelShapeCombo->addItem(QStringLiteral("球体"), QStringLiteral("sphere"));
    m_modelShapeCombo->addItem(QStringLiteral("方块"), QStringLiteral("cube"));
    m_modelShapeCombo->addItem(QStringLiteral("圆柱"), QStringLiteral("cylinder"));
    m_modelShapeCombo->addItem(QStringLiteral("圆锥"), QStringLiteral("cone"));
    m_primarySizeSpin = createAxisSpinBox(modelPage, 1.0, 300.0, 30.0);
    m_secondarySizeSpin = createAxisSpinBox(modelPage, 1.0, 300.0, 24.0);
    m_depthSizeSpin = createAxisSpinBox(modelPage, 1.0, 300.0, 18.0);
    m_resolutionSpin = new QSpinBox(modelPage);
    m_resolutionSpin->setRange(6, 128);
    m_resolutionSpin->setValue(32);
    m_createModelAmbientSpin = createAxisSpinBox(modelPage, 0.0, 1.0, 0.2);
    m_createModelAmbientSpin->setSingleStep(0.05);
    m_createModelDiffuseSpin = createAxisSpinBox(modelPage, 0.0, 1.0, 0.8);
    m_createModelDiffuseSpin->setSingleStep(0.05);
    m_createModelSpecularSpin = createAxisSpinBox(modelPage, 0.0, 1.0, 0.15);
    m_createModelSpecularSpin->setSingleStep(0.05);
    m_createModelSpecularPowerSpin = createAxisSpinBox(modelPage, 0.0, 128.0, 20.0);
    m_createModelSpecularPowerSpin->setSingleStep(1.0);
    m_createModelRoughnessSpin = createAxisSpinBox(modelPage, 0.0, 1.0, 0.4);
    m_createModelRoughnessSpin->setSingleStep(0.05);
    modelForm->addRow(QStringLiteral("基础形体"), m_modelShapeCombo);
    modelForm->addRow(QStringLiteral("主尺寸"), m_primarySizeSpin);
    modelForm->addRow(QStringLiteral("次尺寸"), m_secondarySizeSpin);
    modelForm->addRow(QStringLiteral("深度/高度"), m_depthSizeSpin);
    modelForm->addRow(QStringLiteral("分辨率"), m_resolutionSpin);
    modelForm->addRow(QStringLiteral("Ambient"), m_createModelAmbientSpin);
    modelForm->addRow(QStringLiteral("Diffuse"), m_createModelDiffuseSpin);
    modelForm->addRow(QStringLiteral("Specular"), m_createModelSpecularSpin);
    modelForm->addRow(QStringLiteral("Power"), m_createModelSpecularPowerSpin);
    modelForm->addRow(QStringLiteral("Roughness"), m_createModelRoughnessSpin);
    m_createStack->addWidget(modelPage);

    auto* transformPage = new QWidget(m_createStack);
    auto* transformForm = new QFormLayout(transformPage);
    m_showAxesCheck = new QCheckBox(QStringLiteral("显示坐标轴"), transformPage);
    m_showAxesCheck->setChecked(true);
    m_axesLengthSpin = createAxisSpinBox(transformPage, 1.0, 500.0, 60.0);
    transformForm->addRow(QString(), m_showAxesCheck);
    transformForm->addRow(QStringLiteral("轴长度"), m_axesLengthSpin);
    m_createStack->addWidget(transformPage);

    createLayout->addWidget(m_createStack);

    auto* createButtonsLayout = new QHBoxLayout();
    auto* createButton = new QPushButton(QStringLiteral("创建节点"), createGroup);
    auto* seedButton = new QPushButton(QStringLiteral("生成演示层级"), createGroup);
    createButtonsLayout->addWidget(createButton);
    createButtonsLayout->addWidget(seedButton);
    createLayout->addLayout(createButtonsLayout);
    controlLayout->addWidget(createGroup);

    auto* listGroup = new QGroupBox(QStringLiteral("节点列表"), controlPanel);
    auto* listLayout = new QVBoxLayout(listGroup);
    m_nodeList = new QListWidget(listGroup);
    m_nodeList->setMinimumHeight(160);
    listLayout->addWidget(m_nodeList);
    auto* listButtonsLayout = new QHBoxLayout();
    auto* deleteButton = new QPushButton(QStringLiteral("删除选中节点"), listGroup);
    auto* clearGeometryButton = new QPushButton(QStringLiteral("清空节点数据"), listGroup);
    listButtonsLayout->addWidget(deleteButton);
    listButtonsLayout->addWidget(clearGeometryButton);
    listLayout->addLayout(listButtonsLayout);
    controlLayout->addWidget(listGroup);

    m_displayGroup = new QGroupBox(QStringLiteral("显示属性"), controlPanel);
    m_displayForm = new QFormLayout(m_displayGroup);
    m_visibleCheck = new QCheckBox(QStringLiteral("在 datagen_main 可见"), m_displayGroup);
    m_visibleCheck->setChecked(true);
    m_layerCombo = new QComboBox(m_displayGroup);
    m_layerCombo->addItem(QStringLiteral("第 1 层"), 1);
    m_layerCombo->addItem(QStringLiteral("第 2 层"), 2);
    m_layerCombo->addItem(QStringLiteral("第 3 层"), 3);
    m_redSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.9);
    m_greenSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.5);
    m_blueSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.2);
    m_opacitySpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 1.0);
    m_renderModeCombo = new QComboBox(m_displayGroup);
    m_renderModeCombo->addItem(QStringLiteral("surface"), QStringLiteral("surface"));
    m_renderModeCombo->addItem(QStringLiteral("wireframe"), QStringLiteral("wireframe"));
    m_renderModeCombo->addItem(QStringLiteral("points"), QStringLiteral("points"));
    m_sizeSpin = createAxisSpinBox(m_displayGroup, 1.0, 50.0, 6.0);
    m_materialAmbientSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.2);
    m_materialAmbientSpin->setSingleStep(0.05);
    m_materialDiffuseSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.8);
    m_materialDiffuseSpin->setSingleStep(0.05);
    m_materialSpecularSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.15);
    m_materialSpecularSpin->setSingleStep(0.05);
    m_materialSpecularPowerSpin = createAxisSpinBox(m_displayGroup, 0.0, 128.0, 20.0);
    m_materialSpecularPowerSpin->setSingleStep(1.0);
    m_materialRoughnessSpin = createAxisSpinBox(m_displayGroup, 0.0, 1.0, 0.4);
    m_materialRoughnessSpin->setSingleStep(0.05);
    m_showLabelsCheck = new QCheckBox(QStringLiteral("显示点标签"), m_displayGroup);
    m_showEdgesCheck = new QCheckBox(QStringLiteral("显示模型边线"), m_displayGroup);
    m_dashedCheck = new QCheckBox(QStringLiteral("虚线"), m_displayGroup);
    m_showAxesDisplayCheck = new QCheckBox(QStringLiteral("显示坐标轴"), m_displayGroup);
    m_displayForm->addRow(QString(), m_visibleCheck);
    m_displayForm->addRow(QStringLiteral("图层"), m_layerCombo);
    m_displayForm->addRow(QStringLiteral("红"), m_redSpin);
    m_displayForm->addRow(QStringLiteral("绿"), m_greenSpin);
    m_displayForm->addRow(QStringLiteral("蓝"), m_blueSpin);
    m_displayForm->addRow(QStringLiteral("透明度"), m_opacitySpin);
    m_displayForm->addRow(QStringLiteral("渲染模式"), m_renderModeCombo);
    m_displayForm->addRow(QStringLiteral("点径/线宽"), m_sizeSpin);
    m_displayForm->addRow(QStringLiteral("Ambient"), m_materialAmbientSpin);
    m_displayForm->addRow(QStringLiteral("Diffuse"), m_materialDiffuseSpin);
    m_displayForm->addRow(QStringLiteral("Specular"), m_materialSpecularSpin);
    m_displayForm->addRow(QStringLiteral("Power"), m_materialSpecularPowerSpin);
    m_displayForm->addRow(QStringLiteral("Roughness"), m_materialRoughnessSpin);
    m_displayForm->addRow(QString(), m_showLabelsCheck);
    m_displayForm->addRow(QString(), m_showEdgesCheck);
    m_displayForm->addRow(QString(), m_dashedCheck);
    m_displayForm->addRow(QString(), m_showAxesDisplayCheck);
    m_applyDisplayButton = new QPushButton(QStringLiteral("应用显示属性"), m_displayGroup);
    m_displayForm->addRow(QString(), m_applyDisplayButton);
    controlLayout->addWidget(m_displayGroup);

    m_hierarchyGroup = new QGroupBox(QStringLiteral("父子关系与变换"), controlPanel);
    m_hierarchyForm = new QFormLayout(m_hierarchyGroup);
    m_parentCombo = new QComboBox(m_hierarchyGroup);
    m_translateXSpin = createAxisSpinBox(m_hierarchyGroup, -500.0, 500.0, 0.0);
    m_translateYSpin = createAxisSpinBox(m_hierarchyGroup, -500.0, 500.0, 0.0);
    m_translateZSpin = createAxisSpinBox(m_hierarchyGroup, -500.0, 500.0, 0.0);
    m_rotateXSpin = createAxisSpinBox(m_hierarchyGroup, -180.0, 180.0, 0.0);
    m_rotateYSpin = createAxisSpinBox(m_hierarchyGroup, -180.0, 180.0, 0.0);
    m_rotateZSpin = createAxisSpinBox(m_hierarchyGroup, -180.0, 180.0, 0.0);
    m_hierarchyForm->addRow(QStringLiteral("父变换"), m_parentCombo);
    m_hierarchyForm->addRow(QStringLiteral("位移 X"), m_translateXSpin);
    m_hierarchyForm->addRow(QStringLiteral("位移 Y"), m_translateYSpin);
    m_hierarchyForm->addRow(QStringLiteral("位移 Z"), m_translateZSpin);
    m_hierarchyForm->addRow(QStringLiteral("旋转 X"), m_rotateXSpin);
    m_hierarchyForm->addRow(QStringLiteral("旋转 Y"), m_rotateYSpin);
    m_hierarchyForm->addRow(QStringLiteral("旋转 Z"), m_rotateZSpin);
    auto* hierarchyButtonLayout = new QHBoxLayout();
    m_applyParentButton = new QPushButton(QStringLiteral("设置父变换"), m_hierarchyGroup);
    m_applyTransformButton = new QPushButton(QStringLiteral("应用局部变换"), m_hierarchyGroup);
    m_resetTransformButton = new QPushButton(QStringLiteral("重置局部变换"), m_hierarchyGroup);
    hierarchyButtonLayout->addWidget(m_applyParentButton);
    hierarchyButtonLayout->addWidget(m_applyTransformButton);
    hierarchyButtonLayout->addWidget(m_resetTransformButton);
    m_hierarchyForm->addRow(QString(), hierarchyButtonLayout);
    controlLayout->addWidget(m_hierarchyGroup);

    m_dataGroup = new QGroupBox(QStringLiteral("节点数据编辑"), controlPanel);
    auto* dataLayout = new QVBoxLayout(m_dataGroup);
    m_pointDataForm = new QFormLayout();
    m_pointLabelEdit = new QLineEdit(QStringLiteral("P"), m_dataGroup);
    m_pointXSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_pointYSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_pointZSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_pointDataForm->addRow(QStringLiteral("点标签"), m_pointLabelEdit);
    m_pointDataForm->addRow(QStringLiteral("点 X"), m_pointXSpin);
    m_pointDataForm->addRow(QStringLiteral("点 Y"), m_pointYSpin);
    m_pointDataForm->addRow(QStringLiteral("点 Z"), m_pointZSpin);
    dataLayout->addLayout(m_pointDataForm);
    m_addPointButton = new QPushButton(QStringLiteral("向 PointNode 添加点"), m_dataGroup);
    dataLayout->addWidget(m_addPointButton);

    m_vertexForm = new QFormLayout();
    m_vertexXSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_vertexYSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_vertexZSpin = createAxisSpinBox(m_dataGroup, -500.0, 500.0, 0.0);
    m_vertexForm->addRow(QStringLiteral("顶点 X"), m_vertexXSpin);
    m_vertexForm->addRow(QStringLiteral("顶点 Y"), m_vertexYSpin);
    m_vertexForm->addRow(QStringLiteral("顶点 Z"), m_vertexZSpin);
    dataLayout->addLayout(m_vertexForm);
    m_addVertexButton = new QPushButton(QStringLiteral("向 LineNode 添加顶点"), m_dataGroup);
    dataLayout->addWidget(m_addVertexButton);

    m_detailText = new QTextEdit(m_dataGroup);
    m_detailText->setReadOnly(true);
    m_detailText->setMinimumHeight(110);
    dataLayout->addWidget(m_detailText);
    controlLayout->addWidget(m_dataGroup);
    controlLayout->addStretch(1);

    controlScrollArea->setWidget(controlPanel);

    auto* scenePanel = new QWidget(this);
    auto* scenePanelLayout = new QVBoxLayout(scenePanel);
    scenePanelLayout->setContentsMargins(0, 0, 0, 0);
    scenePanelLayout->setSpacing(8);

    auto* sceneTitle = new QLabel(QStringLiteral("3D Scene"), scenePanel);
    sceneTitle->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 600;"));
    scenePanelLayout->addWidget(sceneTitle);

    auto* sceneContainer = new QWidget(scenePanel);
    m_sceneLayout = new QVBoxLayout(sceneContainer);
    m_sceneLayout->setContentsMargins(0, 0, 0, 0);
    m_sceneLayout->setSpacing(0);
    auto* placeholder = new QLabel(QStringLiteral("3D 数据窗口尚未挂载"), sceneContainer);
    placeholder->setAlignment(Qt::AlignCenter);
    m_sceneLayout->addWidget(placeholder);
    scenePanelLayout->addWidget(sceneContainer, 1);

    rootLayout->addWidget(controlScrollArea, 0);
    rootLayout->addWidget(scenePanel, 1);
    rootLayout->setStretch(0, 0);
    rootLayout->setStretch(1, 1);

    connect(m_createTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                setCreatePanelForNodeType(m_createTypeCombo->currentData().toString());
            });
    connect(createButton, &QPushButton::clicked,
            this, &DataGenPage::onCreateNodeClicked);
    connect(seedButton, &QPushButton::clicked,
            this, &DataGenPage::onSeedDemoClicked);
    connect(deleteButton, &QPushButton::clicked,
            this, &DataGenPage::onDeleteNodeClicked);
    connect(clearGeometryButton, &QPushButton::clicked,
            this, &DataGenPage::onClearGeometryClicked);
    connect(m_nodeList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) {
                onSelectionChanged();
            });
        connect(m_applyDisplayButton, &QPushButton::clicked,
            this, &DataGenPage::onApplyDisplayClicked);
        connect(m_applyParentButton, &QPushButton::clicked,
            this, &DataGenPage::onAssignParentClicked);
        connect(m_applyTransformButton, &QPushButton::clicked,
            this, &DataGenPage::onApplyTransformClicked);
        connect(m_resetTransformButton, &QPushButton::clicked,
            this, &DataGenPage::onResetTransformClicked);
        connect(m_addPointButton, &QPushButton::clicked,
            this, &DataGenPage::onAddPointClicked);
        connect(m_addVertexButton, &QPushButton::clicked,
            this, &DataGenPage::onAddVertexClicked);

    setCreatePanelForNodeType(QStringLiteral("point"));
        updateOperationPanelVisibility(QString());
}

void DataGenPage::setSceneWindow(VtkSceneWindow* sceneWindow)
{
    if (m_sceneWindow == sceneWindow || !m_sceneLayout) {
        return;
    }

    m_sceneWindow = sceneWindow;
    replaceLayoutWidget(m_sceneLayout, m_sceneWindow);
}

void DataGenPage::setActionDispatcher(UiActionDispatcher* dispatcher)
{
    m_actionDispatcher = dispatcher;
}

void DataGenPage::updateModuleState(const QVariantMap& state)
{
    if (!state.contains(QStringLiteral("nodeSummaries"))) {
        return;
    }

    m_nodeSummaries = state.value(QStringLiteral("nodeSummaries")).toList();
    const QString selectedId = state.value(QStringLiteral("selectedNodeId")).toString();
    rebuildNodeList(m_nodeSummaries, selectedId);
    rebuildParentCombo(state.value(QStringLiteral("transformOptions")).toList(),
                       state.value(QStringLiteral("selectedParentTransformId")).toString());
    applySelectedNodeDetails(state.value(QStringLiteral("selectedNodeDetails")).toMap());
    setStatusText(state.value(QStringLiteral("statusText")).toString());
}

void DataGenPage::onCreateNodeClicked()
{
    emitCommand(createCreatePayload());
}

void DataGenPage::onDeleteNodeClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先在列表中选择节点。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("delete_node")},
        {QStringLiteral("nodeId"), selectedNodeId()}
    });
}

void DataGenPage::onSelectionChanged()
{
    const QString nodeId = selectedNodeId();
    if (nodeId.isEmpty()) {
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("select_node")},
        {QStringLiteral("nodeId"), nodeId}
    });
}

void DataGenPage::onApplyDisplayClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择要修改显示属性的节点。"));
        return;
    }

    QVariantMap payload = createDisplayPayload();
    payload.insert(QStringLiteral("command"), QStringLiteral("update_display"));
    payload.insert(QStringLiteral("nodeId"), selectedNodeId());
    emitCommand(payload);
}

void DataGenPage::onAssignParentClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择需要设置父变换的节点。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("assign_parent")},
        {QStringLiteral("nodeId"), selectedNodeId()},
        {QStringLiteral("parentTransformId"), m_parentCombo->currentData().toString()}
    });
}

void DataGenPage::onApplyTransformClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择 TransformNode。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("update_transform_pose")},
        {QStringLiteral("nodeId"), selectedNodeId()},
        {QStringLiteral("tx"), m_translateXSpin->value()},
        {QStringLiteral("ty"), m_translateYSpin->value()},
        {QStringLiteral("tz"), m_translateZSpin->value()},
        {QStringLiteral("rx"), m_rotateXSpin->value()},
        {QStringLiteral("ry"), m_rotateYSpin->value()},
        {QStringLiteral("rz"), m_rotateZSpin->value()}
    });
}

void DataGenPage::onResetTransformClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择 TransformNode。"));
        return;
    }

    m_translateXSpin->setValue(0.0);
    m_translateYSpin->setValue(0.0);
    m_translateZSpin->setValue(0.0);
    m_rotateXSpin->setValue(0.0);
    m_rotateYSpin->setValue(0.0);
    m_rotateZSpin->setValue(0.0);
    onApplyTransformClicked();
}

void DataGenPage::onAddPointClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择 PointNode。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("add_point")},
        {QStringLiteral("nodeId"), selectedNodeId()},
        {QStringLiteral("label"), m_pointLabelEdit->text()},
        {QStringLiteral("x"), m_pointXSpin->value()},
        {QStringLiteral("y"), m_pointYSpin->value()},
        {QStringLiteral("z"), m_pointZSpin->value()}
    });
}

void DataGenPage::onAddVertexClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择 LineNode。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("add_line_vertex")},
        {QStringLiteral("nodeId"), selectedNodeId()},
        {QStringLiteral("x"), m_vertexXSpin->value()},
        {QStringLiteral("y"), m_vertexYSpin->value()},
        {QStringLiteral("z"), m_vertexZSpin->value()}
    });
}

void DataGenPage::onClearGeometryClicked()
{
    if (selectedNodeId().isEmpty()) {
        setStatusText(QStringLiteral("请先选择节点。"));
        return;
    }

    emitCommand({
        {QStringLiteral("command"), QStringLiteral("clear_node_geometry")},
        {QStringLiteral("nodeId"), selectedNodeId()}
    });
}

void DataGenPage::onSeedDemoClicked()
{
    emitCommand({
        {QStringLiteral("command"), QStringLiteral("seed_demo")}
    });
}

QVariantMap DataGenPage::selectedNodeSummary() const
{
    const QString nodeId = selectedNodeId();
    for (const QVariant& item : m_nodeSummaries) {
        const QVariantMap summary = item.toMap();
        if (summary.value(QStringLiteral("id")).toString() == nodeId) {
            return summary;
        }
    }
    return {};
}

QString DataGenPage::selectedNodeId() const
{
    if (!m_nodeList || !m_nodeList->currentItem()) {
        return QString();
    }
    return m_nodeList->currentItem()->data(Qt::UserRole).toString();
}

QVariantMap DataGenPage::createDisplayPayload() const
{
    return {
        {QStringLiteral("visible"), m_visibleCheck->isChecked()},
        {QStringLiteral("layer"), m_layerCombo->currentData().toInt()},
        {QStringLiteral("red"), m_redSpin->value()},
        {QStringLiteral("green"), m_greenSpin->value()},
        {QStringLiteral("blue"), m_blueSpin->value()},
        {QStringLiteral("opacity"), m_opacitySpin->value()},
        {QStringLiteral("renderMode"), m_renderModeCombo->currentData().toString()},
        {QStringLiteral("sizeValue"), m_sizeSpin->value()},
        {QStringLiteral("ambient"), m_materialAmbientSpin->value()},
        {QStringLiteral("diffuse"), m_materialDiffuseSpin->value()},
        {QStringLiteral("specular"), m_materialSpecularSpin->value()},
        {QStringLiteral("specularPower"), m_materialSpecularPowerSpin->value()},
        {QStringLiteral("roughness"), m_materialRoughnessSpin->value()},
        {QStringLiteral("showLabels"), m_showLabelsCheck->isChecked()},
        {QStringLiteral("showEdges"), m_showEdgesCheck->isChecked()},
        {QStringLiteral("dashed"), m_dashedCheck->isChecked()},
        {QStringLiteral("showAxes"), m_showAxesDisplayCheck->isChecked()}
    };
}

QVariantMap DataGenPage::createCreatePayload() const
{
    QVariantMap payload{
        {QStringLiteral("command"), QStringLiteral("create_node")},
        {QStringLiteral("nodeType"), m_createTypeCombo->currentData().toString()},
        {QStringLiteral("name"), m_createNameEdit->text()}
    };

    const QString nodeType = m_createTypeCombo->currentData().toString();
    if (nodeType == QStringLiteral("point")) {
        payload.insert(QStringLiteral("count"), m_initialCountSpin->value());
        payload.insert(QStringLiteral("spacing"), m_spacingSpin->value());
    } else if (nodeType == QStringLiteral("line")) {
        auto* lineCountSpin = m_createStack->widget(1)->findChild<QSpinBox*>(QStringLiteral("lineCountSpin"));
        auto* lineSpacingSpin = m_createStack->widget(1)->findChild<QDoubleSpinBox*>(QStringLiteral("lineSpacingSpin"));
        payload.insert(QStringLiteral("count"), lineCountSpin ? lineCountSpin->value() : 4);
        payload.insert(QStringLiteral("spacing"), lineSpacingSpin ? lineSpacingSpin->value() : 24.0);
        payload.insert(QStringLiteral("closed"), m_closedLineCheck->isChecked());
    } else if (nodeType == QStringLiteral("model")) {
        payload.insert(QStringLiteral("shape"), m_modelShapeCombo->currentData().toString());
        payload.insert(QStringLiteral("sizeA"), m_primarySizeSpin->value());
        payload.insert(QStringLiteral("sizeB"), m_secondarySizeSpin->value());
        payload.insert(QStringLiteral("sizeC"), m_depthSizeSpin->value());
        payload.insert(QStringLiteral("resolution"), m_resolutionSpin->value());
        payload.insert(QStringLiteral("ambient"), m_createModelAmbientSpin->value());
        payload.insert(QStringLiteral("diffuse"), m_createModelDiffuseSpin->value());
        payload.insert(QStringLiteral("specular"), m_createModelSpecularSpin->value());
        payload.insert(QStringLiteral("specularPower"), m_createModelSpecularPowerSpin->value());
        payload.insert(QStringLiteral("roughness"), m_createModelRoughnessSpin->value());
    } else if (nodeType == QStringLiteral("transform")) {
        payload.insert(QStringLiteral("showAxes"), m_showAxesCheck->isChecked());
        payload.insert(QStringLiteral("axesLength"), m_axesLengthSpin->value());
    }

    return payload;
}

void DataGenPage::rebuildNodeList(const QVariantList& nodeSummaries, const QString& selectedNodeId)
{
    QString previousSelection = selectedNodeId;
    if (previousSelection.isEmpty() && m_nodeList && m_nodeList->currentItem()) {
        previousSelection = m_nodeList->currentItem()->data(Qt::UserRole).toString();
    }

    m_nodeList->blockSignals(true);
    m_nodeList->clear();
    int selectedRow = -1;
    int row = 0;
    for (const QVariant& item : nodeSummaries) {
        const QVariantMap summary = item.toMap();
        const QString nodeId = summary.value(QStringLiteral("id")).toString();
        const QString type = nodeTypeLabel(summary.value(QStringLiteral("type")).toString());
        const QString name = summary.value(QStringLiteral("name")).toString();
        const QString parent = summary.value(QStringLiteral("parentName")).toString();
        QString label = QStringLiteral("%1 | %2").arg(type, name);
        if (!parent.isEmpty()) {
            label.append(QStringLiteral(" -> %1").arg(parent));
        }
        auto* widgetItem = new QListWidgetItem(label, m_nodeList);
        widgetItem->setData(Qt::UserRole, nodeId);
        if (nodeId == previousSelection) {
            selectedRow = row;
        }
        ++row;
    }
    if (selectedRow >= 0) {
        m_nodeList->setCurrentRow(selectedRow);
    } else if (m_nodeList->count() > 0) {
        m_nodeList->setCurrentRow(0);
    }
    m_nodeList->blockSignals(false);
}

void DataGenPage::rebuildParentCombo(const QVariantList& transformOptions, const QString& selectedParentId)
{
    m_parentCombo->clear();
    m_parentCombo->addItem(QStringLiteral("无"), QString());
    int selectedIndex = 0;
    int index = 1;
    for (const QVariant& item : transformOptions) {
        const QVariantMap option = item.toMap();
        const QString id = option.value(QStringLiteral("id")).toString();
        const QString name = option.value(QStringLiteral("name")).toString();
        m_parentCombo->addItem(name, id);
        if (id == selectedParentId) {
            selectedIndex = index;
        }
        ++index;
    }
    m_parentCombo->setCurrentIndex(selectedIndex);
}

void DataGenPage::applySelectedNodeDetails(const QVariantMap& details)
{
    if (details.isEmpty()) {
        m_detailText->setPlainText(QStringLiteral("当前无节点详情。"));
        updateOperationPanelVisibility(QString());
        return;
    }

    const QString nodeType = details.value(QStringLiteral("type")).toString();
    const QString name = details.value(QStringLiteral("name")).toString();
    const QString nodeId = details.value(QStringLiteral("id")).toString();
    const QString parentName = details.value(QStringLiteral("parentName")).toString();

    m_visibleCheck->setChecked(details.value(QStringLiteral("visible"), true).toBool());
    const int layer = details.value(QStringLiteral("layer"), 1).toInt();
    const int layerIndex = qMax(0, m_layerCombo->findData(layer));
    m_layerCombo->setCurrentIndex(layerIndex);
    m_redSpin->setValue(details.value(QStringLiteral("red"), 1.0).toDouble());
    m_greenSpin->setValue(details.value(QStringLiteral("green"), 1.0).toDouble());
    m_blueSpin->setValue(details.value(QStringLiteral("blue"), 1.0).toDouble());
    m_opacitySpin->setValue(details.value(QStringLiteral("opacity"), 1.0).toDouble());
    const int renderModeIndex = qMax(0, m_renderModeCombo->findData(
        details.value(QStringLiteral("renderMode"), QStringLiteral("surface")).toString()));
    m_renderModeCombo->setCurrentIndex(renderModeIndex);
    m_sizeSpin->setValue(details.value(QStringLiteral("sizeValue"), 6.0).toDouble());
    m_materialAmbientSpin->setValue(details.value(QStringLiteral("ambient"), 0.2).toDouble());
    m_materialDiffuseSpin->setValue(details.value(QStringLiteral("diffuse"), 0.8).toDouble());
    m_materialSpecularSpin->setValue(details.value(QStringLiteral("specular"), 0.15).toDouble());
    m_materialSpecularPowerSpin->setValue(details.value(QStringLiteral("specularPower"), 20.0).toDouble());
    m_materialRoughnessSpin->setValue(details.value(QStringLiteral("roughness"), 0.4).toDouble());
    m_showLabelsCheck->setChecked(details.value(QStringLiteral("showLabels"), false).toBool());
    m_showEdgesCheck->setChecked(details.value(QStringLiteral("showEdges"), false).toBool());
    m_dashedCheck->setChecked(details.value(QStringLiteral("dashed"), false).toBool());
    m_showAxesDisplayCheck->setChecked(details.value(QStringLiteral("showAxes"), false).toBool());

    m_translateXSpin->setValue(details.value(QStringLiteral("tx"), 0.0).toDouble());
    m_translateYSpin->setValue(details.value(QStringLiteral("ty"), 0.0).toDouble());
    m_translateZSpin->setValue(details.value(QStringLiteral("tz"), 0.0).toDouble());
    m_rotateXSpin->setValue(details.value(QStringLiteral("rx"), 0.0).toDouble());
    m_rotateYSpin->setValue(details.value(QStringLiteral("ry"), 0.0).toDouble());
    m_rotateZSpin->setValue(details.value(QStringLiteral("rz"), 0.0).toDouble());
    updateOperationPanelVisibility(nodeType);

    QString detailText = QStringLiteral("名称: %1\n类型: %2\n节点 ID: %3\n父节点: %4")
        .arg(name,
             nodeTypeLabel(nodeType),
             nodeId,
             parentName.isEmpty() ? QStringLiteral("无") : parentName);

    if (nodeType == QStringLiteral("point")) {
        detailText.append(QStringLiteral("\n点数量: %1").arg(details.value(QStringLiteral("pointCount")).toInt()));
    } else if (nodeType == QStringLiteral("line")) {
        detailText.append(QStringLiteral("\n顶点数量: %1\n总长度: %2")
            .arg(details.value(QStringLiteral("vertexCount")).toInt())
            .arg(details.value(QStringLiteral("length")).toDouble(), 0, 'f', 2));
    } else if (nodeType == QStringLiteral("model")) {
        detailText.append(QStringLiteral("\n模型形体: %1\n面片数量: %2\nAmbient: %3\nDiffuse: %4\nSpecular: %5\nPower: %6\nRoughness: %7")
            .arg(details.value(QStringLiteral("shape"), QStringLiteral("mesh")).toString())
            .arg(details.value(QStringLiteral("triangleCount")).toInt())
            .arg(details.value(QStringLiteral("ambient"), 0.2).toDouble(), 0, 'f', 2)
            .arg(details.value(QStringLiteral("diffuse"), 0.8).toDouble(), 0, 'f', 2)
            .arg(details.value(QStringLiteral("specular"), 0.15).toDouble(), 0, 'f', 2)
            .arg(details.value(QStringLiteral("specularPower"), 20.0).toDouble(), 0, 'f', 2)
            .arg(details.value(QStringLiteral("roughness"), 0.4).toDouble(), 0, 'f', 2));
    } else if (nodeType == QStringLiteral("transform")) {
        detailText.append(QStringLiteral("\n源坐标系: %1\n目标坐标系: %2")
            .arg(details.value(QStringLiteral("sourceSpace"), QStringLiteral("local")).toString())
            .arg(details.value(QStringLiteral("targetSpace"), QStringLiteral("world")).toString()));
    }

    m_detailText->setPlainText(detailText);
}

void DataGenPage::updateOperationPanelVisibility(const QString& nodeType)
{
    const bool hasSelection = !nodeType.isEmpty();
    const bool isPoint = nodeType == QStringLiteral("point");
    const bool isLine = nodeType == QStringLiteral("line");
    const bool isModel = nodeType == QStringLiteral("model");
    const bool isTransform = nodeType == QStringLiteral("transform");

    if (m_displayGroup) {
        m_displayGroup->setVisible(hasSelection);
    }
    if (m_hierarchyGroup) {
        m_hierarchyGroup->setVisible(hasSelection);
    }
    if (m_dataGroup) {
        m_dataGroup->setVisible(hasSelection);
        if (hasSelection) {
            if (isPoint) {
                m_dataGroup->setTitle(QStringLiteral("PointNode 数据编辑"));
            } else if (isLine) {
                m_dataGroup->setTitle(QStringLiteral("LineNode 数据编辑"));
            } else if (isModel) {
                m_dataGroup->setTitle(QStringLiteral("ModelNode 数据概览"));
            } else if (isTransform) {
                m_dataGroup->setTitle(QStringLiteral("TransformNode 数据概览"));
            }
        }
    }

    setFormRowVisible(m_displayForm, m_renderModeCombo, isLine || isModel);
    setFormRowVisible(m_displayForm, m_sizeSpin, isPoint || isLine || isTransform);
    setFormRowVisible(m_displayForm, m_materialAmbientSpin, isModel);
    setFormRowVisible(m_displayForm, m_materialDiffuseSpin, isModel);
    setFormRowVisible(m_displayForm, m_materialSpecularSpin, isModel);
    setFormRowVisible(m_displayForm, m_materialSpecularPowerSpin, isModel);
    setFormRowVisible(m_displayForm, m_materialRoughnessSpin, isModel);
    setFormRowVisible(m_displayForm, m_showLabelsCheck, isPoint);
    setFormRowVisible(m_displayForm, m_showEdgesCheck, isModel);
    setFormRowVisible(m_displayForm, m_dashedCheck, isLine);
    setFormRowVisible(m_displayForm, m_showAxesDisplayCheck, isTransform);
    if (m_applyDisplayButton) {
        m_applyDisplayButton->setVisible(hasSelection);
    }

    setFormRowVisible(m_hierarchyForm, m_parentCombo, hasSelection);
    setFormRowVisible(m_hierarchyForm, m_translateXSpin, isTransform);
    setFormRowVisible(m_hierarchyForm, m_translateYSpin, isTransform);
    setFormRowVisible(m_hierarchyForm, m_translateZSpin, isTransform);
    setFormRowVisible(m_hierarchyForm, m_rotateXSpin, isTransform);
    setFormRowVisible(m_hierarchyForm, m_rotateYSpin, isTransform);
    setFormRowVisible(m_hierarchyForm, m_rotateZSpin, isTransform);
    if (m_applyParentButton) {
        m_applyParentButton->setVisible(hasSelection);
    }
    if (m_applyTransformButton) {
        m_applyTransformButton->setVisible(isTransform);
    }
    if (m_resetTransformButton) {
        m_resetTransformButton->setVisible(isTransform);
    }

    setFormRowVisible(m_pointDataForm, m_pointLabelEdit, isPoint);
    setFormRowVisible(m_pointDataForm, m_pointXSpin, isPoint);
    setFormRowVisible(m_pointDataForm, m_pointYSpin, isPoint);
    setFormRowVisible(m_pointDataForm, m_pointZSpin, isPoint);
    if (m_addPointButton) {
        m_addPointButton->setVisible(isPoint);
    }

    setFormRowVisible(m_vertexForm, m_vertexXSpin, isLine);
    setFormRowVisible(m_vertexForm, m_vertexYSpin, isLine);
    setFormRowVisible(m_vertexForm, m_vertexZSpin, isLine);
    if (m_addVertexButton) {
        m_addVertexButton->setVisible(isLine);
    }
}

void DataGenPage::setCreatePanelForNodeType(const QString& nodeType)
{
    if (nodeType == QStringLiteral("point")) {
        m_createNameEdit->setText(QStringLiteral("Generated Points"));
        m_createStack->setCurrentIndex(0);
    } else if (nodeType == QStringLiteral("line")) {
        m_createNameEdit->setText(QStringLiteral("Generated Path"));
        m_createStack->setCurrentIndex(1);
    } else if (nodeType == QStringLiteral("model")) {
        m_createNameEdit->setText(QStringLiteral("Generated Model"));
        m_createStack->setCurrentIndex(2);
    } else {
        m_createNameEdit->setText(QStringLiteral("Generated Transform"));
        m_createStack->setCurrentIndex(3);
    }
}

void DataGenPage::setStatusText(const QString& text)
{
    if (!text.isEmpty()) {
        m_statusLabel->setText(text);
    }
}

void DataGenPage::emitCommand(const QVariantMap& payload)
{
    if (m_actionDispatcher) {
        m_actionDispatcher->sendCommand(
            payload.value(QStringLiteral("command")).toString(),
            payload);
    }
}