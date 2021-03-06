#ifndef CANVAS_H
#define CANVAS_H

#include <QGLWidget>
#include "tools.h"
#include "camera.h"
#include "document.h"
#include "curvature.h"
#include "shader.h"
#include "texture.h"

enum
{
    MODE_ADD_JOINTS,
    MODE_SCALE_JOINTS,
    MODE_VIEW_MESH,
    MODE_SCULPT_MESH,
    MODE_ANIMATE_MESH,
};

enum
{
    CAMERA_ORBIT,
    CAMERA_FIRST_PERSON,
};

class MeshSculpterTool;

enum
{
    MATERIAL_CURVATURE,
    MATERIAL_MAPLE_CANDY,
    MATERIAL_METAL,
    MATERIAL_RED_WAX,

    NUM_MATERIALS
};

class View : public QGLWidget
{
    Q_OBJECT

public:
    View(QWidget *parent);
    ~View();

    void setMaterial(int material);
    void setBrushMode(int mode);
    void setBrushRadius(float radius);
    void setBrushWeight(float weight);
    void setMode(int mode);
    void setCamera(int camera);
    void setDocument(Document *doc);
    Document &getDocument() { return *doc; }

    void undo();
    void redo();

protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

private:
    Document *doc;
    int selectedBall;
    int oppositeSelectedBall; // used by tools
    int mouseX, mouseY; // for highlighting the face of the selection cube
    int mode;

#ifdef USE_SHADER_MATERIALS
    Shader normalDepthShader;
    Texture normalDepthTexture;
    Texture depthTexture;
    Shader finalCompositeShaders[NUM_MATERIALS];
    int currentMaterial;
#endif

    bool mirrorChanges;
    bool drawWireframe;
    bool drawInterpolated;
    bool drawCurvature;

    int brushMode;
    float brushRadius;
    float brushWeight;
    MeshSculpterTool *brushTool;

    Camera *currentCamera;
    OrbitCamera orbitCamera;
    FirstPersonCamera firstPersonCamera;

    bool drawToolDebug;
    Tool *currentTool;
    QList<Tool *> tools;
    friend class Tool;
    friend class OrbitCameraTool;
    friend class FirstPersonCameraTool;
    friend class MoveSelectionTool;
    friend class ScaleSelectionTool;
    friend class SetAndMoveSelectionTool;
    friend class SetAndScaleSelectionTool;
    friend class CreateBallTool;
    friend class MeshSculpterTool;
    friend class JointRotationTool;

    void clearTools();
    void updateTools();
    void resetCamera();
    void resetInteraction();
    void drawMesh(bool justMesh) const;
    void drawSkeleton(bool drawTransparent) const;
    void drawGroundPlane() const;
    void drawFullscreenQuad() const;
    void camera2D() const;
    void camera3D() const;

public slots:
    void setMirrorChanges(bool useMirrorChanges);
    void setWireframe(bool useWireframe);
    void setInterpolated(bool useInterpolated);
    void setCurvature(bool useCurvature);
    void setDrawToolDebug(bool drawDebug);
    void deleteSelection();
};

#endif // CANVAS_H
