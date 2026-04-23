#pragma once

#include <QWidget>
#include <QVariantMap>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTextEdit;
class QVBoxLayout;
class VtkSceneWindow;
class UiActionDispatcher;

class DataGenPage : public QWidget
{
    Q_OBJECT

public:
    explicit DataGenPage(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);
    void setSceneWindow(VtkSceneWindow* sceneWindow);
    void updateModuleState(const QVariantMap& state);

private slots:
    void onCreateNodeClicked();
    void onDeleteNodeClicked();
    void onSelectionChanged();
    void onApplyDisplayClicked();
    void onAssignParentClicked();
    void onApplyTransformClicked();
    void onResetTransformClicked();
    void onAddPointClicked();
    void onAddVertexClicked();
    void onClearGeometryClicked();
    void onSeedDemoClicked();
    void onPlayProgressPromptOnceClicked();
    void onPlayAttentionPromptOnceClicked();
    void onPlayProgressPromptBurstClicked();
    void onPlayAttentionPromptBurstClicked();

private:
    QVariantMap selectedNodeSummary() const;
    QString selectedNodeId() const;
    QVariantMap createDisplayPayload() const;
    QVariantMap createCreatePayload() const;
    void rebuildNodeList(const QVariantList& nodeSummaries, const QString& selectedNodeId);
    void rebuildParentCombo(const QVariantList& transformOptions, const QString& selectedParentId);
    void applySelectedNodeDetails(const QVariantMap& details);
    void updateOperationPanelVisibility(const QString& nodeType);
    void setCreatePanelForNodeType(const QString& nodeType);
    void setStatusText(const QString& text);
    void emitCommand(const QVariantMap& payload);

    UiActionDispatcher* m_actionDispatcher = nullptr;

    QListWidget* m_nodeList = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTextEdit* m_detailText = nullptr;
    QGroupBox* m_audioGroup = nullptr;
    QGroupBox* m_displayGroup = nullptr;
    QGroupBox* m_hierarchyGroup = nullptr;
    QGroupBox* m_dataGroup = nullptr;
    QFormLayout* m_displayForm = nullptr;
    QFormLayout* m_hierarchyForm = nullptr;
    QFormLayout* m_pointDataForm = nullptr;
    QFormLayout* m_vertexForm = nullptr;

    QComboBox* m_createTypeCombo = nullptr;
    QLineEdit* m_createNameEdit = nullptr;
    QComboBox* m_modelShapeCombo = nullptr;
    QDoubleSpinBox* m_primarySizeSpin = nullptr;
    QDoubleSpinBox* m_secondarySizeSpin = nullptr;
    QDoubleSpinBox* m_depthSizeSpin = nullptr;
    QSpinBox* m_resolutionSpin = nullptr;
    QDoubleSpinBox* m_createModelAmbientSpin = nullptr;
    QDoubleSpinBox* m_createModelDiffuseSpin = nullptr;
    QDoubleSpinBox* m_createModelSpecularSpin = nullptr;
    QDoubleSpinBox* m_createModelSpecularPowerSpin = nullptr;
    QDoubleSpinBox* m_createModelRoughnessSpin = nullptr;
    QSpinBox* m_initialCountSpin = nullptr;
    QDoubleSpinBox* m_spacingSpin = nullptr;
    QCheckBox* m_closedLineCheck = nullptr;
    QCheckBox* m_showAxesCheck = nullptr;
    QDoubleSpinBox* m_axesLengthSpin = nullptr;
    QStackedWidget* m_createStack = nullptr;

    QCheckBox* m_visibleCheck = nullptr;
    QComboBox* m_layerCombo = nullptr;
    QDoubleSpinBox* m_redSpin = nullptr;
    QDoubleSpinBox* m_greenSpin = nullptr;
    QDoubleSpinBox* m_blueSpin = nullptr;
    QDoubleSpinBox* m_opacitySpin = nullptr;
    QComboBox* m_renderModeCombo = nullptr;
    QDoubleSpinBox* m_sizeSpin = nullptr;
    QDoubleSpinBox* m_materialAmbientSpin = nullptr;
    QDoubleSpinBox* m_materialDiffuseSpin = nullptr;
    QDoubleSpinBox* m_materialSpecularSpin = nullptr;
    QDoubleSpinBox* m_materialSpecularPowerSpin = nullptr;
    QDoubleSpinBox* m_materialRoughnessSpin = nullptr;
    QCheckBox* m_showLabelsCheck = nullptr;
    QCheckBox* m_showEdgesCheck = nullptr;
    QCheckBox* m_dashedCheck = nullptr;
    QCheckBox* m_showAxesDisplayCheck = nullptr;
    QPushButton* m_applyDisplayButton = nullptr;

    QComboBox* m_parentCombo = nullptr;
    QDoubleSpinBox* m_translateXSpin = nullptr;
    QDoubleSpinBox* m_translateYSpin = nullptr;
    QDoubleSpinBox* m_translateZSpin = nullptr;
    QDoubleSpinBox* m_rotateXSpin = nullptr;
    QDoubleSpinBox* m_rotateYSpin = nullptr;
    QDoubleSpinBox* m_rotateZSpin = nullptr;
    QPushButton* m_applyParentButton = nullptr;
    QPushButton* m_applyTransformButton = nullptr;
    QPushButton* m_resetTransformButton = nullptr;

    QLineEdit* m_pointLabelEdit = nullptr;
    QDoubleSpinBox* m_pointXSpin = nullptr;
    QDoubleSpinBox* m_pointYSpin = nullptr;
    QDoubleSpinBox* m_pointZSpin = nullptr;
    QDoubleSpinBox* m_vertexXSpin = nullptr;
    QDoubleSpinBox* m_vertexYSpin = nullptr;
    QDoubleSpinBox* m_vertexZSpin = nullptr;
    QPushButton* m_addPointButton = nullptr;
    QPushButton* m_addVertexButton = nullptr;

    VtkSceneWindow* m_sceneWindow = nullptr;
    QVBoxLayout* m_sceneLayout = nullptr;
    QVariantList m_nodeSummaries;
};