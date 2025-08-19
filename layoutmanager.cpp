#include "layoutmanager.h"
#include <QWidget> // Include this header to resolve the incomplete type warning
#include <cmath>

LayoutManager::LayoutManager(QGridLayout* layout)
    : gridLayout(layout), gridRows(0), gridCols(0) {}

void LayoutManager::calculateGridDimensions(int numCameras, int& rows, int& cols) {
    if (numCameras <= 0) return;

    cols = static_cast<int>(std::ceil(std::sqrt(numCameras)));
    rows = static_cast<int>(std::ceil(static_cast<double>(numCameras) / cols));

    // Adjust grid to minimize empty spaces
    while ((rows - 1) * cols >= numCameras) {
        rows--;
    }
    if (rows * cols < numCameras) {
        rows++;
    }

    gridRows = rows;
    gridCols = cols;
}

void LayoutManager::setupLayout(int numCameras) {
    // Clear existing layout
    QLayoutItem* item;
    while ((item = gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // Set row and column stretch factors
    for (int i = 0; i < gridRows; ++i) {
        gridLayout->setRowStretch(i, 1);
    }
    for (int j = 0; j < gridCols; ++j) {
        gridLayout->setColumnStretch(j, 1);
    }
}

