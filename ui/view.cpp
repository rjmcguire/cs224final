#include "view.h"
#include "geometry.h"
#include "curvature.h"
#include "meshsculpter.h"
#include "jointrotation.h"
#include <QWheelEvent>

#define PLANE_SIZE 10
#define CURSOR_SIZE 20

View::View(QWidget *parent) : QGLWidget(parent), doc(new Document), selectedBall(-1), oppositeSelectedBall(-1),
    mouseX(0), mouseY(0), mode(MODE_VIEW_MESH),
#ifdef USE_SHADER_MATERIALS
    currentMaterial(0),
#endif
    mirrorChanges(false), drawWireframe(true), drawInterpolated(true), drawCurvature(false),
    brushMode(BRUSH_ADD_OR_SUBTRACT), brushRadius(0), brushWeight(0), brushTool(NULL),
    currentCamera(&firstPersonCamera), drawToolDebug(false), currentTool(NULL)
{
    resetCamera();
    setMouseTracking(true);
}

View::~View()
{
    delete doc;
    clearTools();
}

void View::setMaterial(int material)
{
#ifdef USE_SHADER_MATERIALS
    currentMaterial = material;
    update();
#endif
}

void View::setBrushMode(int mode)
{
    brushMode = mode;
    if (brushTool) brushTool->brushMode = brushMode;
    update();
}

void View::setBrushRadius(float radius)
{
    brushRadius = radius;
    if (brushTool) brushTool->brushRadius = brushRadius;
    update();
}

void View::setBrushWeight(float weight)
{
    brushWeight = weight;
    if (brushTool) brushTool->brushWeight = brushWeight;
    update();
}

void View::setMode(int newMode)
{
    mode = newMode;
    updateTools();
    update();
}

void View::setCamera(int camera)
{
    resetCamera();
    switch (camera)
    {
    case CAMERA_ORBIT:
        currentCamera = &orbitCamera;
        break;

    case CAMERA_FIRST_PERSON:
        currentCamera = &firstPersonCamera;
        break;
    }
    updateTools();
    update();
}

void View::setDocument(Document *newDoc)
{
    delete doc;
    doc = newDoc;
    resetCamera();
    resetInteraction();
    update();
}

void View::undo()
{
    resetInteraction();
    doc->getUndoStack().undo();
    update();
}

void View::redo()
{
    resetInteraction();
    doc->getUndoStack().redo();
    update();
}

void View::initializeGL()
{
    // opengl lighting
    float ambient0[4] = { 0.4, 0.4, 0.4, 0 };
    float diffuse0[4] = { 0.6, 0.6, 0.6, 0 };
    float diffuse1[4] = { -0.2, -0.2, -0.2, 0 };
    float specular[4] = { 1, 1, 1, 0 };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse1);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64);

    // other opengl state
    glLineWidth(0.5);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_COLOR_MATERIAL);
    glClearColor(0.875, 0.875, 0.875, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonOffset(1, 1);

    // load shaders
#ifdef USE_SHADER_MATERIALS
    normalDepthShader.init(":/shaders/normaldepth.vert", ":/shaders/normaldepth.frag");
    for (int i = 0; i < NUM_MATERIALS; i++)
        finalCompositeShaders[i].init(":/shaders/finalcomposite.vert", ":/shaders/finalcomposite.frag", QString("#define MATERIAL %1").arg(i).toStdString());
#endif
}

void View::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
#ifdef USE_SHADER_MATERIALS
    normalDepthTexture.init(GL_TEXTURE_2D, width, height, GL_RGBA, GL_RGBA32F_ARB, GL_FLOAT, GL_CLAMP_TO_EDGE, GL_NEAREST);
    depthTexture.init(GL_TEXTURE_2D, width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16_ARB, GL_UNSIGNED_SHORT, GL_CLAMP_TO_EDGE, GL_NEAREST);
#endif
}

void View::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera3D();

    // Don't paint if we haven't gotten a resize yet
#ifdef USE_SHADER_MATERIALS
    if (normalDepthTexture.getWidth() * normalDepthTexture.getHeight() == 0)
        return;
#endif

    // position lights
    float position0[4] = { 0, 1, 0, 0 };
    float position1[4] = { 0, -1, 0, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, position0);
    glLightfv(GL_LIGHT1, GL_POSITION, position1);

    if (mode == MODE_SCULPT_MESH)
    {
#ifdef USE_SHADER_MATERIALS
        normalDepthTexture.startDrawingTo(depthTexture);
        normalDepthShader.use();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawMesh(true);
        normalDepthShader.unuse();
        normalDepthTexture.stopDrawingTo();

        camera2D();
        glDepthFunc(GL_ALWAYS);
        normalDepthTexture.bind(0);
        depthTexture.bind(1);
        finalCompositeShaders[currentMaterial].use();
        finalCompositeShaders[currentMaterial].uniform("windowSize", width(), height());
        finalCompositeShaders[currentMaterial].texture("depthTexture", 1);
        drawFullscreenQuad();
        finalCompositeShaders[currentMaterial].unuse();
        depthTexture.unbind(1);
        normalDepthTexture.unbind(0);
        glDepthFunc(GL_LESS);
        camera3D();
#else
        drawMesh(true);
#endif
        drawGroundPlane();
    }
    else if (mode == MODE_VIEW_MESH || mode == MODE_ANIMATE_MESH)
    {
        drawMesh(false);
        drawGroundPlane();
        drawSkeleton(true);
    }
    else
    {
        drawSkeleton(false);
        drawGroundPlane();
    }

    if (drawToolDebug)
        foreach (Tool *tool, tools)
            tool->drawDebug(mouseX, mouseY);
}

void View::mousePressEvent(QMouseEvent *event)
{
    mouseX = event->x();
    mouseY = event->y();

    // old mouse up
    if (currentTool)
    {
        currentTool->mouseReleased(event);
        currentTool = NULL;
    }

    // new mouse down, use first tool to accept mouse event
    foreach (Tool *tool, tools)
    {
        if (tool->mousePressed(event))
        {
            currentTool = tool;
            break;
        }
    }

    update();
}

void View::mouseMoveEvent(QMouseEvent *event)
{
    mouseX = event->x();
    mouseY = event->y();

    if (currentTool)
        currentTool->mouseDragged(event);

    update();
}

void View::mouseReleaseEvent(QMouseEvent *event)
{
    mouseX = event->x();
    mouseY = event->y();

    if (currentTool)
    {
        currentTool->mouseReleased(event);
        currentTool = NULL;
    }

    doc->mesh.updateNormals();

    update();
}

void View::wheelEvent(QWheelEvent *event)
{
    for (int i = 0; i < tools.count(); i++)
    {
        if (tools[i]->wheelEvent(event))
        {
            update();
            break;
        }
    }
}

void View::updateTools()
{
    clearTools();

    switch (mode)
    {
    case MODE_ADD_JOINTS:
        tools += new CreateBallTool(this);
        tools += new MoveSelectionTool(this);
        tools += new SetAndMoveSelectionTool(this);
        break;

    case MODE_SCALE_JOINTS:
        tools += new ScaleSelectionTool(this);
        tools += new SetAndScaleSelectionTool(this);
        break;

    case MODE_SCULPT_MESH:
        brushTool = new MeshSculpterTool(this);
        brushTool->brushMode = brushMode;
        brushTool->brushRadius = brushRadius;
        brushTool->brushWeight = brushWeight;
        tools += brushTool;
        break;

    case MODE_ANIMATE_MESH:
        tools += new JointRotationTool(this);
        break;
    }

    if (currentCamera == &orbitCamera)
        tools += new OrbitCameraTool(this);
    else
        tools += new FirstPersonCameraTool(this);
}

void View::clearTools()
{
    for (int i = 0; i < tools.count(); i++)
        delete tools[i];
    tools.clear();
    brushTool = NULL;
}

void View::resetCamera()
{
    orbitCamera.reset();
    firstPersonCamera.reset();
}

void View::resetInteraction()
{
    currentTool = NULL;
    selectedBall = oppositeSelectedBall = -1;
}

void View::drawMesh(bool justMesh) const
{
    if (doc->mesh.triangles.count() + doc->mesh.quads.count() == 0) return;

    // draw the mesh filled
    glColor3f(0.75, 0.75, 0.75);
    glEnable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_FILL);
    doc->mesh.drawFill();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_LIGHTING);

    // performance is terrible if we draw anything else with framebuffers
    if (justMesh) return;

    if (drawWireframe)
    {
        // enable line drawing
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        // draw the mesh wireframe
        glColor4f(0, 0, 0, 0.5);
        doc->mesh.drawWireframe();

        // draw the vertex points
        glPointSize(3);
        glColor3f(0, 0, 0);
        doc->mesh.drawPoints();

        // disable line drawing
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    if (drawCurvature)
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        glColor4f(0, 0, 0, 0.5);
        Curvature().drawCurvatures(doc->mesh);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void View::drawSkeleton(bool drawTransparent) const
{
    if (doc->mesh.balls.isEmpty()) return;

    // draw model
    if (drawTransparent)
    {
        // set depth buffer before so we never blend the same pixel twice
        glClear(GL_DEPTH_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        doc->mesh.drawKeyBalls();
        if (drawInterpolated) doc->mesh.drawInBetweenBalls();
        else doc->mesh.drawBones();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        // draw blended key balls and bones
        glDepthFunc(GL_EQUAL);
        glEnable(GL_BLEND);
        glEnable(GL_LIGHTING);
        doc->mesh.drawKeyBalls(0.25);
        glColor4f(0.75, 0.75, 0.75, 0.25);
        if (drawInterpolated) doc->mesh.drawInBetweenBalls();
        else doc->mesh.drawBones();
        glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
    }
    else
    {
        // draw key balls and in-between balls
        glEnable(GL_LIGHTING);
        doc->mesh.drawKeyBalls();
        glColor3f(0.75, 0.75, 0.75);
        if (drawInterpolated) doc->mesh.drawInBetweenBalls();
        else doc->mesh.drawBones();
        glDisable(GL_LIGHTING);
    }

    // draw box around selected ball
    if (selectedBall != -1)
    {
        const Ball &selection = doc->mesh.balls[selectedBall];
        float radius = selection.maxRadius();

        // enable line drawing
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        if (mode == MODE_ADD_JOINTS || mode == MODE_ANIMATE_MESH)
        {
            glDisable(GL_DEPTH_TEST);
            glColor4f(0, 0, 0, 0.25);
            drawWireCube(selection.center - radius, selection.center + radius);
            glEnable(GL_DEPTH_TEST);
            glColor3f(0, 0, 0);
            drawWireCube(selection.center - radius, selection.center + radius);

            // find the currently selected cube face and display the cursor
            Raytracer tracer;
            Vector3 ray = tracer.getRayForPixel(mouseX, mouseY);
            HitTest result;
            if (Raytracer::hitTestCube(selection.center - radius, selection.center + radius, currentCamera->eye, ray, result))
            {
                float size = (result.hit - currentCamera->eye).length() * CURSOR_SIZE / height();
                Vector2 angles = result.normal.toAngles();
                glColor3f(0, 0, 0);
                glDisable(GL_DEPTH_TEST);
                glPushMatrix();
                glTranslatef(result.hit.x, result.hit.y, result.hit.z);
                glRotatef(90 - angles.x * 180 / M_PI, 0, 1, 0);
                glRotatef(-angles.y * 180 / M_PI, 1, 0, 0);
                glScalef(size, size, size);
                drawMoveCursor();
                glPopMatrix();
                glEnable(GL_DEPTH_TEST);
            }
        }
        else if (mode == MODE_SCALE_JOINTS)
        {
            // display the cursor
            Raytracer tracer;
            Vector3 ray = tracer.getRayForPixel(mouseX, mouseY);
            HitTest result;
            if (Raytracer::hitTestSphere(selection.center, radius, currentCamera->eye, ray, result))
            {
                camera2D();
                glColor3f(0, 0, 0);
                glDisable(GL_DEPTH_TEST);
                glTranslatef(mouseX, mouseY, 0);
                glScalef(CURSOR_SIZE, CURSOR_SIZE, 0);
                drawScaleCursor();
                glEnable(GL_DEPTH_TEST);
                camera3D();
            }

            Vector3 delta = currentCamera->eye - selection.center;
            Vector2 angles = delta.toAngles();

            // adjust the radius to the profile of the ball as seen from the camera
            radius = radius / sinf(acosf(radius / delta.length()));

            // draw a circle around the selected ball
            radius *= 1.1;
            glPushMatrix();
            glTranslatef(selection.center.x, selection.center.y, selection.center.z);
            glRotatef(90 - angles.x * 180 / M_PI, 0, 1, 0);
            glRotatef(-angles.y * 180 / M_PI, 1, 0, 0);
            glScalef(radius, radius, radius);
            glDisable(GL_DEPTH_TEST);
            glColor4f(0, 0, 0, 0.25);
            drawWireDisk();
            glEnable(GL_DEPTH_TEST);
            glColor3f(0, 0, 0);
            drawWireDisk();
            glPopMatrix();
        }

        // disable line drawing
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void View::drawGroundPlane() const
{
    // enable line drawing
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    glBegin(GL_LINES);
    glColor3f(0, 0, 0);
    glVertex3i(0, 0, PLANE_SIZE);
    glVertex3i(-PLANE_SIZE, 0, 0);
    glVertex3i(0, 0, PLANE_SIZE);
    glVertex3i(PLANE_SIZE, 0, 0);
    glVertex3i(-PLANE_SIZE / 2, 0, -PLANE_SIZE);
    glVertex3i(-PLANE_SIZE / 2, 0, 0);
    glVertex3i(PLANE_SIZE / 2, 0, -PLANE_SIZE);
    glVertex3i(PLANE_SIZE / 2, 0, 0);
    glVertex3i(-PLANE_SIZE / 2, 0, -PLANE_SIZE);
    glVertex3i(PLANE_SIZE / 2, 0, -PLANE_SIZE);
    for (int z = 1 - PLANE_SIZE; z < PLANE_SIZE; z++)
    {
        int size = (z >= 0) ? z : PLANE_SIZE / 2;
        int xmin = -PLANE_SIZE + size;
        int xmax = PLANE_SIZE - size;
        glColor4f(0, 0, 0, z == 0 ? 1 : 0.25);
        glVertex3i(xmin, 0, z);
        glVertex3i(xmax, 0, z);
    }
    for (int x = 1 - PLANE_SIZE; x < PLANE_SIZE; x++)
    {
        int zmin = (abs(x) < PLANE_SIZE / 2) ? -PLANE_SIZE : 0;
        int zmax = PLANE_SIZE - abs(x);
        glColor4f(0, 0, 0, x == 0 ? 1 : 0.25);
        glVertex3i(x, 0, zmin);
        glVertex3i(x, 0, zmax);
    }
    glEnd();

    // disable line drawing
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void View::drawFullscreenQuad() const
{
    int w = width(), h = height();
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 1); glVertex2i(0, 0);
    glTexCoord2i(0, 0); glVertex2i(0, h);
    glTexCoord2i(1, 0); glVertex2i(w, h);
    glTexCoord2i(1, 1); glVertex2i(w, 0);
    glEnd();
}

void View::camera2D() const
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width(), height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void View::camera3D() const
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, (float)width() / (float)height(), 0.1, 5000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    currentCamera->apply();
}

void View::setMirrorChanges(bool useMirrorChanges)
{
    mirrorChanges = useMirrorChanges;
    update();
}

void View::setWireframe(bool useWireframe)
{
    drawWireframe = useWireframe;
    update();
}

void View::setInterpolated(bool useInterpolated)
{
    drawInterpolated = useInterpolated;
    update();
}

void View::setCurvature(bool useCurvature) {
    drawCurvature = useCurvature;
    update();
}

void View::setDrawToolDebug(bool drawDebug)
{
    drawToolDebug = drawDebug;
    update();
}

void View::deleteSelection()
{
    if (selectedBall != -1)
    {
        // finish the interaction before deletion
        if (currentTool)
        {
            QMouseEvent event(QEvent::MouseButtonRelease, QPoint(mouseX, mouseY), Qt::LeftButton, 0, 0);
            currentTool->mouseReleased(&event);
            currentTool = NULL;
        }

        doc->deleteBall(selectedBall);
        selectedBall = -1;
        update();
    }
}
