#ifndef LAYOUTMANAGER_H
#define LAYOUTMANAGER_H

#include <QGridLayout>
#include <vector>

class LayoutManager {
public:
    LayoutManager(QGridLayout* layout);
    void calculateGridDimensions(int numCameras, int& rows, int& cols);
    void setupLayout(int numCameras);

private:
    QGridLayout* gridLayout;
    int gridRows;
    int gridCols;
};

#endif // LAYOUTMANAGER_H
