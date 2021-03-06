#ifndef MESHSCULPTER_H
#define MESHSCULPTER_H

#include "tools.h"
#include "meshacceleration.h"
#include "meshinfo.h"

enum
{
    BRUSH_ADD_OR_SUBTRACT,
    BRUSH_SMOOTH,
    BRUSH_GRAB,
    BRUSH_SNAKE,
};

/**
 * MeshSculpterTool must derive from QObject to use the verticesChanged function.
 * However, it cannot use multiple inheritance (even if QObject is the first
 * superclass as the documentation says) because when trying to delete a
 * MeshSculpterTool the allocator exits with the error "pointer being freed
 * was not allocated". The solution is to make Tool inherit from QObject instead,
 * which only uses single inheritance.
 */
class MeshSculpterTool : public Tool
{
    Q_OBJECT

private:
    MetaMesh *mesh;
    AccelerationDataStructure *accel;
    bool isRightButton;

    // Remember info about the mesh so we can tell when it has changed
    MeshInfo meshInfo;

    // List of vertices that will be added to the ChangeVerticesCommand on mouse up
    QSet<MetaVertex *> verticesToCommit;

    Vector3 grabbedCenter;
    Vector3 grabbedNormal;
    QSet<MetaVertex *> grabbedVertices;
    QVector<Vector3> snakePositions;

    Vector3 interpolateAlongSnake(float t);
    void updateAccel();
    void getVerticesInSphere(const Vector3 &center, float radius, QSet<MetaVertex *> &vertices);
    void stampBrush(const Vector3 &brushCenter, const Vector3 &brushNormal);
    void moveGrabbedVertices(int x, int y);
    void moveSnake(int x, int y);
    void commitChanges(QSet<Quad *> &quadsNeedingNormals);

public:
    float brushRadius;
    float brushWeight;
    int brushMode;

    MeshSculpterTool(View *view);
    ~MeshSculpterTool();

    void drawDebug(int x, int y);
    bool mousePressed(QMouseEvent *event);
    void mouseDragged(QMouseEvent *event);
    void mouseReleased(QMouseEvent *event);

public slots:
    void verticesChanged(const QVector<int> &vertexIndices);
};

#endif // MESHSCULPTER_H
