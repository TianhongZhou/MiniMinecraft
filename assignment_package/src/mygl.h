#ifndef MYGL_H
#define MYGL_H

#include "openglcontext.h"
#include "shaderprogram.h"
#include "scene/worldaxes.h"
#include "scene/camera.h"
#include "scene/terrain.h"
#include "scene/player.h"

#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <smartpointerhelp.h>
#include "scene/blocktypeworker.h"
#include "scene/vboworker.h"
#include "framebuffer.h"
#include "scene/quad.h"


class MyGL : public OpenGLContext
{
    Q_OBJECT
private:
    WorldAxes m_worldAxes; // A wireframe representation of the world axes. It is hard-coded to sit centered at (32, 128, 32).
    ShaderProgram m_progLambert;// A shader program that uses lambertian reflection
    ShaderProgram m_progFlat;// A shader program that uses "flat" reflection (no shadowing at all)
    ShaderProgram m_progInstanced;// A shader program that is designed to be compatible with instanced rendering
    ShaderProgram m_progPost;

    GLuint vao; // A handle for our vertex array object. This will store the VBOs created in our geometry classes.
                // Don't worry too much about this. Just know it is necessary in order to render geometry.

    Terrain m_terrain; // All of the Chunks that currently comprise the world.
    Player m_player; // The entity controlled by the user. Contains a camera to display what it sees as well.
    InputBundle m_inputs; // A collection of variables to be updated in keyPressEvent, mouseMoveEvent, mousePressEvent, etc.

    QTimer m_timer; // Timer linked to tick(). Fires approximately 60 times per second.

    qint64 lastT;
    InputBundle input;

    void moveMouseToCenter(); // Forces the mouse position to the screen's center. You should call this
                              // from within a mouse move event after reading the mouse movement so that
                              // your mouse stays within the screen bounds and is always read.

    void sendPlayerDataToGUI() const;
    GLuint m_texture;
    float m_time;

    FrameBuffer m_frameBuffer;
    Quad m_quad;

    GLuint depthTexture;
    GLuint shadowMap;


public:
    explicit MyGL(QWidget *parent = nullptr);
    ~MyGL();

    // Called once when MyGL is initialized.
    // Once this is called, all OpenGL function
    // invocations are valid (before this, they
    // will cause segfaults)
    void initializeGL() override;
    // Called whenever MyGL is resized.
    void resizeGL(int w, int h) override;
    // Called whenever MyGL::update() is called.
    // In the base code, update() is called from tick().
    void paintGL() override;

    // Called from paintGL().
    // Calls Terrain::draw().
    void renderTerrain(ShaderProgram shader);

protected:
    // Automatically invoked when the user
    // presses a key on the keyboard
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    // Automatically invoked when the user
    // moves the mouse
    void mouseMoveEvent(QMouseEvent *e) override;
    // Automatically invoked when the user
    // presses a mouse button
    void mousePressEvent(QMouseEvent *e) override;

private slots:
    void tick(); // Slot that gets called ~60 times per second by m_timer firing.

signals:
    void sig_sendPlayerPos(QString) const;
    void sig_sendPlayerVel(QString) const;
    void sig_sendPlayerAcc(QString) const;
    void sig_sendPlayerLook(QString) const;
    void sig_sendPlayerChunk(QString) const;
    void sig_sendPlayerTerrainZone(QString) const;
};


#endif // MYGL_H
