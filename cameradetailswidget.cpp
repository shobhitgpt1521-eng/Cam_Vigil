#include "cameradetailswidget.h"
#include <QFormLayout>
#include <QLabel>
#include <QDebug>
#include <vector>

CameraDetailsWidget::CameraDetailsWidget(CameraManager* cameraManager, QWidget* parent)
    : QWidget(parent),
      cameraManager(cameraManager),
      currentCameraIndex(-1)
{
    // Use a form layout for clear label-field alignment.
    QFormLayout* formLayout = new QFormLayout(this);
    // Change label alignment to left
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignHCenter);

    // Create and style the "Cameras" label
    QLabel* cameraLabel = new QLabel("Cameras");
    cameraLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");

    // Set up the camera combo box
    cameraCombo = new QComboBox(this);
    cameraCombo->setStyleSheet("font-size: 18px; font-weight: bold; color: white; background-color: #4d4d4d;");
    cameraCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Populate the dropdown with camera profiles.
    std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i)
    {
        cameraCombo->addItem(QString::fromStdString(profiles[i].displayName), i);
    }
    connect(cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraDetailsWidget::onCameraSelectionChanged);

    // Create and style the "Name" label
    QLabel* nameLabel = new QLabel("Name");
    nameLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");

    // Set up the name edit box
    nameEdit = new QLineEdit(this);
    nameEdit->setStyleSheet("font-size: 18px; font-weight: bold; color: white; background-color: #4d4d4d;");
    nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(nameEdit, &QLineEdit::editingFinished, this, &CameraDetailsWidget::onNameEditingFinished);

    // Add rows with custom labels
    formLayout->addRow(cameraLabel, cameraCombo);
    formLayout->addRow(nameLabel, nameEdit);

    setLayout(formLayout);

    // Load info for the first camera if available.
    if (!profiles.empty())
    {
        cameraCombo->setCurrentIndex(0);
        onCameraSelectionChanged(0);
    }
}

void CameraDetailsWidget::loadCameraInfo(int cameraIndex)
{
    std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
    if (cameraIndex < 0 || cameraIndex >= static_cast<int>(profiles.size()))
        return;
    nameEdit->setText(QString::fromStdString(profiles[cameraIndex].displayName));
}

void CameraDetailsWidget::onCameraSelectionChanged(int comboIndex)
{
    int cameraIndex = cameraCombo->itemData(comboIndex).toInt();
    currentCameraIndex = cameraIndex;
    loadCameraInfo(currentCameraIndex);
}

void CameraDetailsWidget::onNameEditingFinished()
{
    if (currentCameraIndex < 0)
        return;
    QString newName = nameEdit->text();
    cameraManager->renameCamera(currentCameraIndex, newName.toStdString());
    int comboIndex = cameraCombo->currentIndex();
    if (comboIndex >= 0)
    {
        cameraCombo->setItemText(comboIndex, newName);
    }
}
