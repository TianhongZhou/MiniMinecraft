#include "mygl.h"
#include "qdatetime.h"
#include <glm_includes.h>

#include <iostream>
#include <QApplication>
#include <QKeyEvent>
#include <mutex>
#include <thread>


MyGL::MyGL(QWidget *parent)
    : OpenGLContext(parent),
      m_worldAxes(this),
      m_progLambert(this), m_progFlat(this), m_progInstanced(this), m_progPost(this),
      m_terrain(this), m_player(glm::vec3(48.f, 180.f, 48.f), m_terrain), lastT(QDateTime::currentMSecsSinceEpoch()),
      input(*mkU<InputBundle>()), m_time(0.f),
      m_frameBuffer(this, width(), height(), devicePixelRatio()), m_quad(this),
      m_shadowMap(this, 1024, 1024, 1)
{
    // Connect the timer to a function so that when the timer ticks the function is executed
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    // Tell the timer to redraw 60 times per second
    m_timer.start(16);
    setFocusPolicy(Qt::ClickFocus);

    setMouseTracking(true); // MyGL will track the mouse's movements even if a mouse button is not pressed
    setCursor(Qt::BlankCursor); // Make the cursor invisible
}

MyGL::~MyGL() {
    makeCurrent();
    glDeleteVertexArrays(1, &vao);
}


void MyGL::moveMouseToCenter() {
    QCursor::setPos(this->mapToGlobal(QPoint(width() / 2, height() / 2)));
}

void MyGL::initializeGL()
{
    // Create an OpenGL context using Qt's QOpenGLFunctions_3_2_Core class
    // If you were programming in a non-Qt context you might use GLEW (GL Extension Wrangler)instead
    initializeOpenGLFunctions();
    // Print out some information about the current OpenGL context
    debugContextVersion();

    // Set a few settings/modes in OpenGL rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // Set the color with which the screen is filled at the start of each render call.
    glClearColor(0.37f, 0.74f, 1.0f, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printGLErrorLog();

    // Create a Vertex Attribute Object
    glGenVertexArrays(1, &vao);

    //Create the instance of the world axes
    m_worldAxes.createVBOdata();
    m_quad.createVBOdata();

    // Create and set up the diffuse shader
    m_progLambert.create(":/glsl/lambert.vert.glsl", ":/glsl/lambert.frag.glsl");
    // Create and set up the flat lighting shader
    m_progFlat.create(":/glsl/flat.vert.glsl", ":/glsl/flat.frag.glsl");
    m_progInstanced.create(":/glsl/instanced.vert.glsl", ":/glsl/lambert.frag.glsl");
    m_progPost.create(":/glsl/post.vert.glsl", ":/glsl/post.frag.glsl");

    // We have to have a VAO bound in OpenGL 3.2 Core. But if we're not
    // using multiple VAOs, we can just bind one once.
    glBindVertexArray(vao);

    m_terrain.CreateInitialScene();

    std::thread blockTypeWorker(&BlockTypeWorker::operator(), BlockTypeWorker(&m_terrain));
    blockTypeWorker.detach();
    std::thread vboWorker(&VBOWorker::operator(), VBOWorker(&m_terrain));
    vboWorker.detach();

    glGenTextures(1, &m_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    QString texturePath = "../textures/minecraft_textures_all.png";
    QImage img(texturePath);
    img = (img.convertToFormat(QImage::Format_RGBA8888)).mirrored();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

    m_progLambert.setUnifSampler2D("u_Texture", 0);

    m_frameBuffer.create();
    m_frameBuffer.bindFrameBuffer();

    m_progLambert.setUnifVec3("u_FogColor", glm::vec3(0.8f, 0.8f, 0.8f));
    m_progLambert.setUnifFloat("u_FogDensity", 0.01f);
}


void MyGL::resizeGL(int w, int h) {
    //This code sets the concatenated view and perspective projection matrices used for
    //our scene's camera view.
    m_player.setCameraWidthHeight(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    glm::mat4 viewproj = m_player.mcr_camera.getViewProj();

    // Upload the view-projection matrix to our shaders (i.e. onto the graphics card)

    m_progLambert.setUnifMat4("u_View", m_player.mcr_camera.getView());
    m_progLambert.setUnifMat4("u_ViewProj", viewproj);
    m_progFlat.setUnifMat4("u_ViewProj", viewproj);
    m_progInstanced.setUnifMat4("u_ViewProj", viewproj);

    m_frameBuffer.resize(width(), height(), devicePixelRatio());
    m_frameBuffer.destroy();
    m_frameBuffer.create();

    printGLErrorLog();
}


// MyGL's constructor links tick() to a timer that fires 60 times per second.
// We're treating MyGL as our game engine class, so we're going to perform
// all per-frame actions here, such as performing physics updates on all
// entities in the scene.
void MyGL::tick() {
    float dt = QDateTime::currentMSecsSinceEpoch() - lastT;
    lastT = QDateTime::currentMSecsSinceEpoch();
    m_player.tick(dt, input);

    // Expand terrain if needed
    glm::vec3 playerPos = m_player.mcr_position;
    int xCenter = glm::floor(playerPos.x / 64) * 64;
    int zCenter = glm::floor(playerPos.z / 64) * 64;

    std::lock_guard<std::mutex> lock(m_terrain.dataToBlockTypeWorkerThreadsMutex);

    for (int xOffset = -128; xOffset <= 128; xOffset += 64) {
        for (int zOffset = -128; zOffset <= 128; zOffset += 64) {
            int x = xCenter + xOffset;
            int z = zCenter + zOffset;

            if (m_terrain.m_generatedTerrain.find(toKey(x, z)) == m_terrain.m_generatedTerrain.end()) {
                m_terrain.m_generatedTerrain.insert(toKey(x, z));

                for (int dx = 0; dx < 64; dx += 16) {
                    for (int dz = 0; dz < 64; dz += 16) {
                        int chunkX = x + dx;
                        int chunkZ = z + dz;

                        Chunk* newChunk = m_terrain.instantiateChunkAt(chunkX, chunkZ);
                        m_terrain.dataToBlockTypeWorkerThreads.push_back(newChunk);
                    }
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock2(m_terrain.dataFromBlockTypeWorkerThreads.lockForResource);
    if (!m_terrain.dataFromBlockTypeWorkerThreads.sharedResource.empty()) {
        for (Chunk* processedChunk : m_terrain.dataFromBlockTypeWorkerThreads.sharedResource) {
            m_terrain.dataToVBOWorkerThreads.push_back(processedChunk);
        }
        m_terrain.dataFromBlockTypeWorkerThreads.sharedResource.clear();
    }

    update(); // Calls paintGL() as part of a larger QOpenGLWidget pipeline
    sendPlayerDataToGUI(); // Updates the info in the secondary window displaying player data
}

void MyGL::sendPlayerDataToGUI() const {
    emit sig_sendPlayerPos(m_player.posAsQString());
    emit sig_sendPlayerVel(m_player.velAsQString());
    emit sig_sendPlayerAcc(m_player.accAsQString());
    emit sig_sendPlayerLook(m_player.lookAsQString());
    glm::vec2 pPos(m_player.mcr_position.x, m_player.mcr_position.z);
    glm::ivec2 chunk(16 * glm::ivec2(glm::floor(pPos / 16.f)));
    glm::ivec2 zone(64 * glm::ivec2(glm::floor(pPos / 64.f)));
    emit sig_sendPlayerChunk(QString::fromStdString("( " + std::to_string(chunk.x) + ", " + std::to_string(chunk.y) + " )"));
    emit sig_sendPlayerTerrainZone(QString::fromStdString("( " + std::to_string(zone.x) + ", " + std::to_string(zone.y) + " )"));
}

// This function is called whenever update() is called.
// MyGL's constructor links update() to a timer that fires 60 times per second,
// so paintGL() called at a rate of 60 frames per second.
void MyGL::paintGL() {
    glm::vec3 lightTarget = glm::vec3(48.f, 0.f, 48.f);
    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.5f, -1.f, -0.75f));
    glm::vec3 lightPos = lightTarget - lightDir * 300.f;

    float sceneSize = 20.f;
    glm::mat4 lightProjection = glm::ortho(-sceneSize, sceneSize, -sceneSize, sceneSize, 1.0f, 500.f);
    glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.f, 1.f, 0.f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    m_progFlat.setUnifMat4("u_LightSpaceMatrix", lightSpaceMatrix);

    m_progFlat.useMe();
    renderTerrain(m_progFlat);

    m_frameBuffer.bindFrameBuffer();
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    // Clear the screen so that we only see newly drawn images
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 viewproj = m_player.mcr_camera.getViewProj();
    m_progLambert.setUnifMat4("u_ViewProj", viewproj);
    m_progFlat.setUnifMat4("u_ViewProj", viewproj);
    m_progInstanced.setUnifMat4("u_ViewProj", viewproj);
    m_progLambert.setUnifMat4("u_Model", glm::mat4());
    m_progLambert.setUnifMat4("u_LightSpaceMatrix", lightSpaceMatrix);
    m_progLambert.setUnifVec3("u_LightDir", -lightDir);

    glm::vec3 cameraPos = glm::floor(m_player.mcr_position + glm::vec3(0.f, 1.5f, 0.f));
    BlockType current = m_terrain.getGlobalBlockAt(cameraPos);

    if (current == WATER || current == LAVA) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    m_progLambert.setUnifFloat("u_Time", m_time++);

    m_progLambert.useMe();
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glActiveTexture(GL_TEXTURE0);

    renderTerrain(m_progLambert);

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_frameBuffer.bindToTextureSlot(1);

    m_progPost.useMe();
    if (current == WATER) {
        m_progPost.setUnifInt("u_Water", 1);
    } else if (current == LAVA) {
        m_progPost.setUnifInt("u_Water", 0);
    } else {
        m_progPost.setUnifInt("u_Water", -1);
    }
    m_progPost.draw(m_quad, 1);

    glDisable(GL_DEPTH_TEST);

    glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);
    m_progFlat.setUnifMat4("u_ViewProj", ortho);

    m_progFlat.setUnifMat4("u_Model", glm::mat4(1.0f));
    m_progFlat.draw(m_worldAxes);

    glEnable(GL_DEPTH_TEST);
}

// TODO: Change this so it renders the nine zones of generated
// terrain that surround the player (refer to Terrain::m_generatedTerrain
// for more info)
void MyGL::renderTerrain(ShaderProgram shader) {
    glm::vec3 currPos = m_player.mcr_position;
    int currX = glm::floor(currPos.x / 16.f);
    int currZ = glm::floor(currPos.z / 16.f);
    int x = 16 * currX;
    int z = 16 * currZ;
    m_terrain.draw(x - 96, x + 97, z - 96, z + 97, &shader);
}


void MyGL::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        QApplication::quit();
    }

    if (e->key() == Qt::Key_W) {
        input.wPressed = true;
    }

    if (e->key() == Qt::Key_A) {
        input.aPressed = true;
    }

    if (e->key() == Qt::Key_S) {
        input.sPressed = true;
    }

    if (e->key() == Qt::Key_D) {
        input.dPressed = true;
    }

    if (e->key() == Qt::Key_Q) {
        input.qPressed = true;
    }

    if (e->key() == Qt::Key_E) {
        input.ePressed = true;
    }

    if (e->key() == Qt::Key_F && !e->isAutoRepeat()) {
        input.fPressed = true;
    }

    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        input.spacePressed = true;
    }

    if (e->key() == Qt::Key_Space) {
        input.spaceHold = true;
    }
}

void MyGL::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_W) {
        input.wPressed = false;
    }

    if (e->key() == Qt::Key_A) {
        input.aPressed = false;
    }

    if (e->key() == Qt::Key_S) {
        input.sPressed = false;
    }

    if (e->key() == Qt::Key_D) {
        input.dPressed = false;
    }

    if (e->key() == Qt::Key_Q) {
        input.qPressed = false;
    }

    if (e->key() == Qt::Key_E) {
        input.ePressed = false;
    }

    if (e->key() == Qt::Key_F && !e->isAutoRepeat()) {
        input.fPressed = false;
    }

    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        input.spacePressed = false;
    }

    if (e->key() == Qt::Key_Space) {
        input.spaceHold = false;
    }
}

void MyGL::mouseMoveEvent(QMouseEvent *e) {
    QPoint center = mapToGlobal(QPoint(width() / 2, height() / 2));

    float deltaX = e->pos().x() - width() / 2;
    float deltaY = e->pos().y() - height() / 2;

    input.mouseX = deltaX;
    input.mouseY = deltaY;

    QCursor::setPos(center);
}

void MyGL::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_player.updateFacingBlock();
        if (m_player.showB) {
            m_player.showB = false;
            glm::vec3 block = m_player.facingBlock;
            BlockType t = m_terrain.getGlobalBlockAt(block.x, block.y, block.z);

            if (t == BEDROCK) {
                return;
            }

            m_terrain.setGlobalBlockAt(block.x, block.y, block.z, EMPTY);
        }
    } else if (e->button() == Qt::RightButton) {
        m_player.updateFacingBlock();
        if (m_player.showB) {
            m_player.showB = false;
            glm::vec3 block = m_player.facingBlock;
            BlockType t = m_terrain.getGlobalBlockAt(block.x, block.y, block.z);

            glm::vec3 origin = m_player.mcr_camera.mcr_position;
            glm::vec3 ray = m_player.getForward();

            glm::vec3 invDir = 1.0f / ray;
            glm::vec3 tMin = (block - origin) * invDir;
            glm::vec3 tMax = (block + glm::vec3(1.0f) - origin) * invDir;

            glm::vec3 t1 = glm::min(tMin, tMax);
            glm::vec3 t2 = glm::max(tMin, tMax);

            float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
            float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);

            if (tNear < tFar && tFar > 0) {
                glm::vec3 hitPoint = origin + tNear * ray;
                glm::vec3 normal;

                if (fabs(hitPoint.x - block.x) < 0.001f) {
                    normal = glm::vec3(-1, 0, 0);
                } else if (fabs(hitPoint.x - (block.x + 1.0f)) < 0.001f) {
                    normal = glm::vec3(1, 0, 0);
                } else if (fabs(hitPoint.y - block.y) < 0.001f) {
                    normal = glm::vec3(0, -1, 0);
                } else if (fabs(hitPoint.y - (block.y + 1.0f)) < 0.001f) {
                    normal = glm::vec3(0, 1, 0);
                } else if (fabs(hitPoint.z - block.z) < 0.001f) {
                    normal = glm::vec3(0, 0, -1);
                } else if (fabs(hitPoint.z - (block.z + 1.0f)) < 0.001f) {
                    normal = glm::vec3(0, 0, 1);
                }

                glm::vec3 newBlockPos = block + normal;
                glm::vec3 playerPos = glm::floor(m_player.mcr_position);
                glm::vec3 playerPosPlus1 = playerPos;
                playerPosPlus1.y = playerPosPlus1.y + 1;
                if (m_terrain.getGlobalBlockAt(newBlockPos.x, newBlockPos.y, newBlockPos.z) == EMPTY &&
                    newBlockPos != playerPos && newBlockPos != playerPosPlus1) {
                    m_terrain.setGlobalBlockAt(newBlockPos.x, newBlockPos.y, newBlockPos.z, t);
                }
            }
        }
    }
}
