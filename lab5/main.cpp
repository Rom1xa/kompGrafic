#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

const float PI = 3.14159265f;
const int FLOATS_PER_VERTEX = 8;

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

static bool g_textured = true;
static bool g_lightingOn = true;

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

static GLuint g_textures[6] = {0, 0, 0, 0, 0, 0};

static void buildCube() {
  g_faces.clear();
  float h = 0.5f;
  Vec3 V[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
               {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
  int idx[6][4] = {
      {4, 5, 6, 7}, // +Z front
      {1, 0, 3, 2}, // -Z back
      {5, 1, 2, 6}, // +X right
      {0, 4, 7, 3}, // -X left
      {7, 6, 2, 3}, // +Y top
      {0, 1, 5, 4}, // -Y bottom
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

static const float UV[4][2] = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};

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
      int idx = tri[k];
      const Vec3 &vp = p[idx];
      out.push_back(vp.x);
      out.push_back(vp.y);
      out.push_back(vp.z);
      out.push_back(f.normal.x);
      out.push_back(f.normal.y);
      out.push_back(f.normal.z);
      out.push_back(UV[idx][0]);
      out.push_back(UV[idx][1]);
    }
  }
}

static void drawArraysVNT(const float *data, int vertexCount) {
  if (vertexCount <= 0)
    return;
  size_t stride = FLOATS_PER_VERTEX * sizeof(float);
  glVertexPointer(3, GL_FLOAT, stride, data);
  glNormalPointer(GL_FLOAT, stride, data + 3);
  glTexCoordPointer(2, GL_FLOAT, stride, data + 6);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

struct Image {
  int w = 0, h = 0;
  std::vector<unsigned char> rgba;
};

static bool loadBMP24(const char *path, Image &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;
  unsigned char hdr[54];
  f.read((char *)hdr, 54);
  if (!f || hdr[0] != 'B' || hdr[1] != 'M')
    return false;
  uint32_t dataOffset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) |
                        ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
  int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) |
                        ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
  int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) |
                        ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
  uint16_t bpp = (uint16_t)((uint32_t)hdr[28] | ((uint32_t)hdr[29] << 8));
  uint32_t comp = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) |
                  ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);
  if (bpp != 24 || comp != 0 || w <= 0 || h == 0)
    return false;
  bool flipped = (h < 0);
  if (flipped)
    h = -h;
  f.seekg(dataOffset, std::ios::beg);
  int rowSize = ((24 * w + 31) / 32) * 4;
  std::vector<unsigned char> raw((size_t)rowSize * (size_t)h);
  f.read((char *)raw.data(), (std::streamsize)raw.size());
  if (!f)
    return false;
  out.w = w;
  out.h = h;
  out.rgba.assign((size_t)w * (size_t)h * 4u, 255);
  for (int y = 0; y < h; ++y) {
    int srcRow = flipped ? (h - 1 - y) : y;
    const unsigned char *row = &raw[(size_t)srcRow * (size_t)rowSize];
    unsigned char *dst = &out.rgba[(size_t)y * (size_t)w * 4u];
    for (int x = 0; x < w; ++x) {
      unsigned char b = row[x * 3 + 0];
      unsigned char g = row[x * 3 + 1];
      unsigned char r = row[x * 3 + 2];
      dst[x * 4 + 0] = r;
      dst[x * 4 + 1] = g;
      dst[x * 4 + 2] = b;
      dst[x * 4 + 3] = 255;
    }
  }
  return true;
}

static void putPixel(Image &img, int x, int y, unsigned char r, unsigned char g,
                     unsigned char b) {
  if (x < 0 || y < 0 || x >= img.w || y >= img.h)
    return;
  size_t i = ((size_t)y * (size_t)img.w + (size_t)x) * 4u;
  img.rgba[i + 0] = r;
  img.rgba[i + 1] = g;
  img.rgba[i + 2] = b;
  img.rgba[i + 3] = 255;
}

static void fillCircle(Image &img, int cx, int cy, int rad, unsigned char r,
                       unsigned char g, unsigned char b) {
  int r2 = rad * rad;
  for (int y = std::max(0, cy - rad); y <= std::min(img.h - 1, cy + rad); ++y)
    for (int x = std::max(0, cx - rad); x <= std::min(img.w - 1, cx + rad);
         ++x) {
      int dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= r2)
        putPixel(img, x, y, r, g, b);
    }
}

static void makeDieFaceTexture(int faceIndex, int size, Image &out) {
  out.w = size;
  out.h = size;
  out.rgba.assign((size_t)size * (size_t)size * 4u, 255);
  unsigned char bg[6][3] = {
      {235, 90, 90},   // 1 - красный
      {90, 200, 100},  // 2 - зелёный
      {90, 130, 230},  // 3 - синий
      {235, 200, 80},  // 4 - жёлтый
      {220, 100, 200}, // 5 - сиреневый
      {90, 215, 215},  // 6 - бирюзовый
  };
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x)
      putPixel(out, x, y, bg[faceIndex][0], bg[faceIndex][1], bg[faceIndex][2]);
  int b = size / 18;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      if (x < b || x >= size - b || y < b || y >= size - b)
        putPixel(out, x, y, 30, 30, 35);
    }
  int n = faceIndex + 1;
  int q = size / 4;
  int c = size / 2;
  int rad = size / 14;
  int pts[6][2];
  int count = 0;
  switch (n) {
  case 1:
    pts[count][0] = c;
    pts[count][1] = c;
    ++count;
    break;
  case 2:
    pts[count][0] = q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = 3 * q;
    ++count;
    break;
  case 3:
    pts[count][0] = q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = c;
    pts[count][1] = c;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = 3 * q;
    ++count;
    break;
  case 4:
    pts[count][0] = q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = q;
    pts[count][1] = 3 * q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = 3 * q;
    ++count;
    break;
  case 5:
    pts[count][0] = q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = c;
    pts[count][1] = c;
    ++count;
    pts[count][0] = q;
    pts[count][1] = 3 * q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = 3 * q;
    ++count;
    break;
  case 6:
    pts[count][0] = q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = q;
    ++count;
    pts[count][0] = q;
    pts[count][1] = c;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = c;
    ++count;
    pts[count][0] = q;
    pts[count][1] = 3 * q;
    ++count;
    pts[count][0] = 3 * q;
    pts[count][1] = 3 * q;
    ++count;
    break;
  }
  for (int i = 0; i < count; ++i) {
    fillCircle(out, pts[i][0], pts[i][1], rad + 2, 30, 30, 35);
    fillCircle(out, pts[i][0], pts[i][1], rad, 245, 245, 245);
  }
}

static GLuint uploadTexture(const Image &img) {
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, img.w, img.h, GL_RGBA,
                    GL_UNSIGNED_BYTE, img.rgba.data());
  return id;
}

static void loadAllTextures() {
  for (int i = 0; i < 6; ++i) {
    char path[64];
    std::snprintf(path, sizeof(path), "textures/%d.bmp", i + 1);
    Image img;
    if (loadBMP24(path, img)) {
      printf("[tex] face %d: загружен %s (%dx%d)\n", i + 1, path, img.w, img.h);
    } else {
      makeDieFaceTexture(i, 256, img);
      printf("[tex] face %d: процедурная (нет файла %s)\n", i + 1, path);
    }
    g_textures[i] = uploadTexture(img);
  }
}

static void drawCubeEdges(float explode) {
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
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
  if (g_lightingOn)
    glEnable(GL_LIGHTING);
}

static void drawLightSource(float lx, float ly, float lz) {
  bool wasLit = glIsEnabled(GL_LIGHTING);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glPushMatrix();
  glTranslatef(lx, ly, lz);
  glColor3f(1.0f, 0.95f, 0.55f);
  glutSolidSphere(0.07, 20, 20);
  glPopMatrix();
  glColor3f(0.4f, 0.4f, 0.25f);
  glBegin(GL_LINES);
  glVertex3f(0.f, 0.f, 0.f);
  glVertex3f(lx, ly, lz);
  glEnd();
  if (wasLit)
    glEnable(GL_LIGHTING);
}

static void setupLighting() {
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
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static void drawOneFace(const float *allVerts, int faceIdx) {
  if (g_textured) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_textures[faceIdx]);
    if (g_transparent)
      glColor4f(1.f, 1.f, 1.f, g_alpha);
    else
      glColor4f(1.f, 1.f, 1.f, 1.f);
  } else {
    glDisable(GL_TEXTURE_2D);
    if (g_transparent)
      glColor4f(0.55f, 0.70f, 0.90f, g_alpha);
    else
      glColor4f(0.55f, 0.70f, 0.90f, 1.f);
  }
  drawArraysVNT(allVerts + (size_t)faceIdx * 6 * FLOATS_PER_VERTEX, 6);
}

static void drawCubeAllFaces(const float *allVerts) {
  for (int i = 0; i < 6; ++i)
    drawOneFace(allVerts, i);
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

  if (g_lightingOn) {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
  } else {
    glDisable(GL_LIGHTING);
  }

  std::vector<float> verts;
  buildCubeVertexArray(verts, g_explode);

  if (g_transparent) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    drawCubeAllFaces(verts.data());
    glCullFace(GL_BACK);
    drawCubeAllFaces(verts.data());
    glDisable(GL_CULL_FACE);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  } else {
    drawCubeAllFaces(verts.data());
  }

  glDisable(GL_TEXTURE_2D);
  drawCubeEdges(g_explode);
  drawLightSource(lx, ly, lz);

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
  printf("Если файлы текстур не найдены — используется процедурная текстура "
         "(грань кубика).\n");
  printf("Управление:\n");
  printf("  X          : вкл/выкл текстуры\n");
  printf("  L          : вкл/выкл освещение\n");
  printf("  T          : вкл/выкл прозрачность граней\n");
  printf("  , / .      : уменьшить / увеличить альфу\n");
  printf("  +/=  -/_   : раздвижение граней (отключает авто)\n");
  printf("  A          : вкл/выкл автоматическое 'дыхание' граней\n");
  printf("  Space      : пауза вращения света\n");
  printf("  [ / ]      : скорость света\n");
  printf("  Стрелки    : вращать камеру\n");
  printf("  W / S      : приблизить / отдалить\n");
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
  case 'x':
  case 'X':
    g_textured = !g_textured;
    printf("[ui] текстуры: %s\n", g_textured ? "ON" : "OFF");
    break;
  case 'l':
  case 'L':
    g_lightingOn = !g_lightingOn;
    printf("[ui] освещение: %s\n", g_lightingOn ? "ON" : "OFF");
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
    g_textured = true;
    g_lightingOn = true;
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
  glutCreateWindow("Lab5 - 3D figure with textures, lighting, transparency");

  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_NORMALIZE);

  buildCube();
  setupLighting();
  glEnable(GL_LIGHT0);
  setupMaterial();
  loadAllTextures();

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
