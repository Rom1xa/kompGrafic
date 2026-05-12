#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <vector>

const float PI = 3.14159265f;
const int FLOATS_PER_VERTEX = 6;

static float g_lightAngle = 0.f;
static float g_lightSpeed = 1.0f;
static float g_lightHeight = 0.9f;
static float g_lightRadius = 2.5f;

static float g_explode = 0.f;
static float g_explodeMax = 0.9f;
static bool g_autoExplode = true;
static float g_autoT = 0.f;

static bool g_paused = false;

static bool g_transparent = false;
static float g_alpha = 0.40f;

static float g_camYaw = 0.6f;
static float g_camPitch = 0.35f;
static float g_camDist = 4.5f;

static bool g_mouseDown = false;
static int g_mouseX = 0, g_mouseY = 0;

static int g_lastMs = 0;

struct Vec3 {
  float x, y, z;
};

struct Face {
  Vec3 normal;
  Vec3 v[4];
};

static std::vector<Face> g_faces;

static void buildCube() {
  g_faces.clear();
  float h = 0.5f;
  Vec3 V[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
               {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
  int idx[6][4] = {
      {4, 5, 6, 7}, // front  +Z
      {1, 0, 3, 2}, // back   -Z
      {5, 1, 2, 6}, // right  +X
      {0, 4, 7, 3}, // left   -X
      {7, 6, 2, 3}, // top    +Y
      {0, 1, 5, 4}, // bottom -Y
  };
  Vec3 normals[6] = {{0, 0, 1},  {0, 0, -1}, {1, 0, 0},
                     {-1, 0, 0}, {0, 1, 0},  {0, -1, 0}};
  for (int i = 0; i < 6; ++i) {
    Face f;
    f.normal = normals[i];
    for (int j = 0; j < 4; ++j)
      f.v[j] = V[idx[i][j]];
    g_faces.push_back(f);
  }
}

static void buildCubeVertexArray(std::vector<float> &out, float explode) {
  out.clear();
  out.reserve(g_faces.size() * 6 * FLOATS_PER_VERTEX);
  for (const Face &f : g_faces) {
    Vec3 off = {f.normal.x * explode, f.normal.y * explode,
                f.normal.z * explode};
    Vec3 p[4];
    for (int i = 0; i < 4; ++i)
      p[i] = {f.v[i].x + off.x, f.v[i].y + off.y, f.v[i].z + off.z};
    int tri[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) {
      const Vec3 &vp = p[tri[k]];
      out.push_back(vp.x);
      out.push_back(vp.y);
      out.push_back(vp.z);
      out.push_back(f.normal.x);
      out.push_back(f.normal.y);
      out.push_back(f.normal.z);
    }
  }
}

static void drawArraysVN(const float *data, int vertexCount) {
  if (vertexCount <= 0)
    return;
  glVertexPointer(3, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), data);
  glNormalPointer(GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), data + 3);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

static void drawCubeEdges(float explode) {
  glDisable(GL_LIGHTING);
  glColor3f(0.05f, 0.08f, 0.12f);
  glLineWidth(1.5f);
  for (const Face &f : g_faces) {
    Vec3 off = {f.normal.x * explode, f.normal.y * explode,
                f.normal.z * explode};
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 4; ++i) {
      glVertex3f(f.v[i].x + off.x, f.v[i].y + off.y, f.v[i].z + off.z);
    }
    glEnd();
  }
  glEnable(GL_LIGHTING);
}

static void drawLightBulbSphere(float lx, float ly, float lz) {
  glPushMatrix();
  glTranslatef(lx, ly, lz);
  glColor3f(1.0f, 0.95f, 0.55f);
  glutSolidSphere(0.07, 20, 20);
  glPopMatrix();
}

static void drawLightSource(float lx, float ly, float lz) {
  glDisable(GL_LIGHTING);
  drawLightBulbSphere(lx, ly, lz);
  glColor3f(0.4f, 0.4f, 0.25f);
  glBegin(GL_LINES);
  glVertex3f(0.f, 0.f, 0.f);
  glVertex3f(lx, ly, lz);
  glEnd();
  glEnable(GL_LIGHTING);
}

static void setupLighting() {
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

  GLfloat globalAmb[] = {0.15f, 0.15f, 0.18f, 1.f};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);

  GLfloat lAmb[] = {0.10f, 0.10f, 0.12f, 1.f};
  GLfloat lDif[] = {1.00f, 0.95f, 0.85f, 1.f};
  GLfloat lSpec[] = {1.00f, 1.00f, 1.00f, 1.f};
  glLightfv(GL_LIGHT0, GL_AMBIENT, lAmb);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lDif);
  glLightfv(GL_LIGHT0, GL_SPECULAR, lSpec);

  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05f);
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.02f);
}

static void setupMaterial() {
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

  GLfloat matSpec[] = {0.6f, 0.6f, 0.7f, 1.f};
  GLfloat matShin[] = {48.f};
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matShin);
}

static void display() {
  glClearColor(0.06f, 0.07f, 0.10f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  float cx = g_camDist * cosf(g_camPitch) * sinf(g_camYaw);
  float cy = g_camDist * sinf(g_camPitch);
  float cz = g_camDist * cosf(g_camPitch) * cosf(g_camYaw);
  gluLookAt(cx, cy, cz, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);

  float lx = g_lightRadius * cosf(g_lightAngle);
  float ly = g_lightHeight;
  float lz = g_lightRadius * sinf(g_lightAngle);
  GLfloat lightPos[4] = {lx, ly, lz, 1.f};
  glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

  std::vector<float> verts;
  buildCubeVertexArray(verts, g_explode);
  int vertexCount = (int)(verts.size() / FLOATS_PER_VERTEX);

  float vlx = lx - cx, vly = ly - cy, vlz = lz - cz;
  float distLightSq = vlx * vlx + vly * vly + vlz * vlz;
  float distCubeCenterSq = g_camDist * g_camDist;
  bool sunBehindCube = distLightSq > distCubeCenterSq;

  glEnable(GL_LIGHTING);

  if (g_transparent) {
    if (sunBehindCube) {
      glDisable(GL_LIGHTING);
      drawLightBulbSphere(lx, ly, lz);
      glEnable(GL_LIGHTING);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.55f, 0.70f, 0.90f, g_alpha);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    drawArraysVN(verts.data(), vertexCount);
    glCullFace(GL_BACK);
    drawArraysVN(verts.data(), vertexCount);
    glDisable(GL_CULL_FACE);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    drawCubeEdges(g_explode);

    if (sunBehindCube) {
      glDisable(GL_LIGHTING);
      glColor3f(0.4f, 0.4f, 0.25f);
      glBegin(GL_LINES);
      glVertex3f(0.f, 0.f, 0.f);
      glVertex3f(lx, ly, lz);
      glEnd();
      glEnable(GL_LIGHTING);
    } else {
      drawLightSource(lx, ly, lz);
    }
  } else {
    glColor3f(0.55f, 0.70f, 0.90f);
    drawArraysVN(verts.data(), vertexCount);
    drawCubeEdges(g_explode);
    drawLightSource(lx, ly, lz);
  }

  glutSwapBuffers();
}

static void timer(int) {
  int now = glutGet(GLUT_ELAPSED_TIME);
  float dt = (now - g_lastMs) * 0.001f;
  if (dt > 0.1f)
    dt = 0.1f;
  g_lastMs = now;

  if (!g_paused) {
    g_lightAngle += g_lightSpeed * dt;
    if (g_lightAngle > 2.f * PI)
      g_lightAngle -= 2.f * PI;

    if (g_autoExplode) {
      g_autoT += dt * 0.6f;
      g_explode = (0.5f - 0.5f * cosf(g_autoT)) * g_explodeMax;
    }
  }

  glutPostRedisplay();
  glutTimerFunc(16, timer, 0);
}

static void reshape(int w, int h) {
  if (h <= 0)
    h = 1;
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = (float)w / (float)h;
  gluPerspective(55.0, aspect, 0.1, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void printHelp() {
  printf("Управление:\n");
  printf("  +/=        : увеличить раздвижение граней (отключает авто)\n");
  printf("  -/_        : уменьшить раздвижение граней (отключает авто)\n");
  printf("  A          : вкл/выкл автоматическое 'дыхание' граней\n");
  printf("  T          : вкл/выкл прозрачность граней\n");
  printf("  , / .      : уменьшить / увеличить альфу (прозрачность)\n");
  printf("  Space      : пауза/продолжить вращение света\n");
  printf("  Стрелки    : вращать камеру\n");
  printf("  W / S      : приблизить / отдалить\n");
  printf("  [ / ]      : изменить скорость вращения света\n");
  printf("  R          : сброс\n");
  printf("  Esc / Q    : выход\n");
}

static void keyboard(unsigned char key, int, int) {
  switch (key) {
  case '+':
  case '=':
    g_autoExplode = false;
    g_explode += 0.05f;
    if (g_explode > 2.0f)
      g_explode = 2.0f;
    break;
  case '-':
  case '_':
    g_autoExplode = false;
    g_explode -= 0.05f;
    if (g_explode < 0.f)
      g_explode = 0.f;
    break;
  case 'a':
  case 'A':
    g_autoExplode = !g_autoExplode;
    if (g_autoExplode) {
      float v = g_explode / g_explodeMax;
      if (v < 0.f)
        v = 0.f;
      if (v > 1.f)
        v = 1.f;
      g_autoT = acosf(1.f - 2.f * v);
    }
    break;
  case ' ':
    g_paused = !g_paused;
    break;
  case 't':
  case 'T':
    g_transparent = !g_transparent;
    break;
  case ',':
  case '<':
    g_alpha -= 0.05f;
    if (g_alpha < 0.05f)
      g_alpha = 0.05f;
    break;
  case '.':
  case '>':
    g_alpha += 0.05f;
    if (g_alpha > 1.0f)
      g_alpha = 1.0f;
    break;
  case 'w':
  case 'W':
    g_camDist -= 0.2f;
    if (g_camDist < 1.5f)
      g_camDist = 1.5f;
    break;
  case 's':
  case 'S':
    g_camDist += 0.2f;
    if (g_camDist > 20.f)
      g_camDist = 20.f;
    break;
  case '[':
    g_lightSpeed -= 0.2f;
    if (g_lightSpeed < 0.f)
      g_lightSpeed = 0.f;
    break;
  case ']':
    g_lightSpeed += 0.2f;
    if (g_lightSpeed > 5.f)
      g_lightSpeed = 5.f;
    break;
  case 'r':
  case 'R':
    g_lightAngle = 0.f;
    g_lightSpeed = 1.0f;
    g_explode = 0.f;
    g_autoT = 0.f;
    g_autoExplode = true;
    g_camYaw = 0.6f;
    g_camPitch = 0.35f;
    g_camDist = 4.5f;
    g_paused = false;
    g_transparent = false;
    g_alpha = 0.40f;
    break;
  case 'q':
  case 'Q':
  case 27:
    glutLeaveMainLoop();
    break;
  }
}

static void special(int key, int, int) {
  switch (key) {
  case GLUT_KEY_LEFT:
    g_camYaw -= 0.08f;
    break;
  case GLUT_KEY_RIGHT:
    g_camYaw += 0.08f;
    break;
  case GLUT_KEY_UP:
    g_camPitch += 0.06f;
    if (g_camPitch > 1.45f)
      g_camPitch = 1.45f;
    break;
  case GLUT_KEY_DOWN:
    g_camPitch -= 0.06f;
    if (g_camPitch < -1.45f)
      g_camPitch = -1.45f;
    break;
  }
}

static void mouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON) {
    g_mouseDown = (state == GLUT_DOWN);
    g_mouseX = x;
    g_mouseY = y;
  } else if (button == 3) {
    if (state == GLUT_DOWN) {
      g_camDist -= 0.3f;
      if (g_camDist < 1.5f)
        g_camDist = 1.5f;
    }
  } else if (button == 4) {
    if (state == GLUT_DOWN) {
      g_camDist += 0.3f;
      if (g_camDist > 20.f)
        g_camDist = 20.f;
    }
  }
}

static void motion(int x, int y) {
  if (!g_mouseDown)
    return;
  int dx = x - g_mouseX;
  int dy = y - g_mouseY;
  g_mouseX = x;
  g_mouseY = y;
  g_camYaw += dx * 0.01f;
  g_camPitch += dy * 0.01f;
  if (g_camPitch > 1.45f)
    g_camPitch = 1.45f;
  if (g_camPitch < -1.45f)
    g_camPitch = -1.45f;
}

int main(int argc, char **argv) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(900, 700);
  glutCreateWindow(
      "Lab4 - 3D figure, exploding faces, rotating light, transparency");

  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_NORMALIZE);

  buildCube();
  setupLighting();
  setupMaterial();

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutSpecialFunc(special);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);

  g_lastMs = glutGet(GLUT_ELAPSED_TIME);
  glutTimerFunc(16, timer, 0);

  printHelp();
  glutMainLoop();
  return 0;
}
