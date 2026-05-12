#include <GL/freeglut.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unistd.h>
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

static uint32_t readU32LE(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint16_t readU16LE(const unsigned char *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void paletteBGRToRGBA(unsigned char idx, const unsigned char *palette,
                             int palEntries, unsigned char *rgbaOut) {
  size_t pi = (size_t)idx * 4u;
  if ((int)idx >= palEntries)
    pi = 0;
  rgbaOut[0] = palette[pi + 2];
  rgbaOut[1] = palette[pi + 1];
  rgbaOut[2] = palette[pi + 0];
  rgbaOut[3] = 255;
}

static bool decodeRLE8(const unsigned char *src, size_t srcLen, int w, int h,
                       bool topDown, const unsigned char *palette,
                       int palEntries, Image &out) {
  out.w = w;
  out.h = h;
  out.rgba.assign((size_t)w * (size_t)h * 4u, 0);
  auto putIdx = [&](int x, int bmpScanline, unsigned char idx) {
    if (x < 0 || x >= w || bmpScanline < 0 || bmpScanline >= h)
      return;
    int imgY = topDown ? bmpScanline : (h - 1 - bmpScanline);
    paletteBGRToRGBA(idx, palette, palEntries,
                     &out.rgba[(size_t)imgY * (size_t)w * 4u + (size_t)x * 4u]);
  };

  size_t pos = 0;
  int x = 0, bmpScanline = 0;
  while (pos + 2 <= srcLen && bmpScanline < h) {
    unsigned char n = src[pos++];
    unsigned char v = src[pos++];
    if (n > 0) {
      for (int i = 0; i < (int)n; ++i) {
        putIdx(x, bmpScanline, v);
        x++;
      }
    } else if (v == 0) {
      x = 0;
      bmpScanline++;
    } else if (v == 1) {
      break;
    } else if (v == 2) {
      if (pos + 2 > srcLen)
        return false;
      x += src[pos++];
      bmpScanline += src[pos++];
    } else {
      int count = (int)v;
      for (int i = 0; i < count; ++i) {
        if (pos >= srcLen)
          return false;
        putIdx(x, bmpScanline, src[pos++]);
        x++;
      }
      if (count & 1)
        pos++;
    }
  }
  return true;
}

static bool loadBMP(const char *path, Image &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;

  unsigned char fh[14];
  f.read((char *)fh, 14);
  if (!f || fh[0] != 'B' || fh[1] != 'M')
    return false;
  uint32_t bfOffBits = readU32LE(fh + 10);

  f.seekg(14, std::ios::beg);
  uint32_t dibSize = 0;
  f.read((char *)&dibSize, 4);
  if (!f || dibSize < 40u || dibSize > 256u * 1024u)
    return false;

  std::vector<unsigned char> dib(dibSize);
  memcpy(dib.data(), &dibSize, 4);
  f.read((char *)dib.data() + 4, (std::streamsize)dibSize - 4);
  if (!f)
    return false;

  int32_t w = (int32_t)readU32LE(dib.data() + 4);
  int32_t rawH = (int32_t)readU32LE(dib.data() + 8);
  uint16_t planes = readU16LE(dib.data() + 12);
  uint16_t bpp = readU16LE(dib.data() + 14);
  uint32_t compression = readU32LE(dib.data() + 16);
  uint32_t clrUsed = readU32LE(dib.data() + 32);

  if (planes != 1 || w <= 0 || rawH == 0)
    return false;

  bool topDown = rawH < 0;
  int h = topDown ? -rawH : rawH;

  int palEntries = 0;
  if (bpp <= 8u) {
    palEntries = (int)clrUsed;
    if (palEntries <= 0)
      palEntries = 1 << bpp;
    if (palEntries > 256)
      palEntries = 256;
  }

  std::vector<unsigned char> palette;
  if (bpp <= 8u) {
    palette.assign((size_t)palEntries * 4u, 0);
    std::streamoff palStart = (std::streamoff)(14 + dibSize);
    std::streamoff pixStart = (std::streamoff)bfOffBits;
    if (pixStart > palStart) {
      size_t maxRead = (size_t)(pixStart - palStart);
      size_t want = (size_t)palEntries * 4u;
      size_t nread = want < maxRead ? want : maxRead;
      f.seekg(palStart);
      f.read((char *)palette.data(), (std::streamsize)nread);
    }
  }

  f.seekg(0, std::ios::end);
  std::streamoff fileEnd = f.tellg();
  if (fileEnd < (std::streamoff)bfOffBits)
    return false;
  size_t pixLen = (size_t)(fileEnd - (std::streamoff)bfOffBits);
  std::vector<unsigned char> pix(pixLen);
  f.seekg((std::streamoff)bfOffBits, std::ios::beg);
  f.read((char *)pix.data(), (std::streamsize)pixLen);
  if (!f && pixLen > 0)
    return false;

  out.w = w;
  out.h = h;
  out.rgba.assign((size_t)w * (size_t)h * 4u, 255);

  if (bpp == 24u && compression == 0u) {
    int rowSize = ((24 * w + 31) / 32) * 4;
    if (pix.size() < (size_t)rowSize * (size_t)h)
      return false;
    for (int y = 0; y < h; ++y) {
      int srcRow = topDown ? y : (h - 1 - y);
      const unsigned char *row = &pix[(size_t)srcRow * (size_t)rowSize];
      unsigned char *dst = &out.rgba[(size_t)y * (size_t)w * 4u];
      for (int x = 0; x < w; ++x) {
        dst[x * 4 + 0] = row[x * 3 + 2];
        dst[x * 4 + 1] = row[x * 3 + 1];
        dst[x * 4 + 2] = row[x * 3 + 0];
        dst[x * 4 + 3] = 255;
      }
    }
    return true;
  }

  if (bpp == 32u && (compression == 0u || compression == 3u)) {
    int rowSize = w * 4;
    if (pix.size() < (size_t)rowSize * (size_t)h)
      return false;
    for (int y = 0; y < h; ++y) {
      int srcRow = topDown ? y : (h - 1 - y);
      const unsigned char *row = &pix[(size_t)srcRow * (size_t)rowSize];
      unsigned char *dst = &out.rgba[(size_t)y * (size_t)w * 4u];
      for (int x = 0; x < w; ++x) {
        dst[x * 4 + 0] = row[x * 4 + 2];
        dst[x * 4 + 1] = row[x * 4 + 1];
        dst[x * 4 + 2] = row[x * 4 + 0];
        unsigned char a = row[x * 4 + 3];
        dst[x * 4 + 3] = a ? a : 255;
      }
    }
    return true;
  }

  if (bpp == 8u && compression == 0u) {
    int rowSize = ((8 * w + 31) / 32) * 4;
    if (pix.size() < (size_t)rowSize * (size_t)h)
      return false;
    for (int y = 0; y < h; ++y) {
      int srcRow = topDown ? y : (h - 1 - y);
      const unsigned char *row = &pix[(size_t)srcRow * (size_t)rowSize];
      unsigned char *dst = &out.rgba[(size_t)y * (size_t)w * 4u];
      for (int x = 0; x < w; ++x)
        paletteBGRToRGBA(row[x], palette.data(), palEntries, dst + x * 4);
    }
    return true;
  }

  if (bpp == 8u && compression == 1u)
    return decodeRLE8(pix.data(), pix.size(), w, h, topDown, palette.data(),
                      palEntries, out);

  return false;
}

static Image makeFallbackTexture() {
  Image img;
  img.w = img.h = 1;
  img.rgba = {200, 60, 200, 255};
  return img;
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

static void textureExecutableDir(char *out, size_t outSz, char **argv) {
  out[0] = '\0';
#ifdef __linux__
  if (outSz == 0)
    return;
  ssize_t n = readlink("/proc/self/exe", out, outSz - 1);
  if (n > 0) {
    out[n] = '\0';
    char *slash = strrchr(out, '/');
    if (slash)
      *slash = '\0';
    else
      std::snprintf(out, outSz, ".");
    return;
  }
#endif
  if (argv && argv[0] && argv[0][0]) {
    std::snprintf(out, outSz, "%s", argv[0]);
    char *slash = strrchr(out, '/');
    if (slash && slash != out)
      *slash = '\0';
    else
      std::snprintf(out, outSz, ".");
    return;
  }
  std::snprintf(out, outSz, ".");
}

static void loadAllTextures(char **argv) {
  char exeDir[512];
  textureExecutableDir(exeDir, sizeof(exeDir), argv);

  static const char *rel[] = {"textures", "lab5/textures"};
  for (int i = 0; i < 6; ++i) {
    Image img;
    char path[768];
    bool ok = false;
    const char *roots[] = {exeDir, "."};
    for (size_t ri = 0; ri < sizeof(roots) / sizeof(roots[0]); ++ri) {
      if (ri > 0 && strcmp(roots[ri], roots[ri - 1]) == 0)
        continue;
      for (const char *sub : rel) {
        std::snprintf(path, sizeof(path), "%s/%s/%d.bmp", roots[ri], sub,
                      i + 1);
        if (loadBMP(path, img)) {
          ok = true;
          printf("[tex] грань %d: %s (%dx%d)\n", i + 1, path, img.w, img.h);
          break;
        }
      }
      if (ok)
        break;
    }
    if (!ok) {
      fprintf(
          stderr,
          "[tex] ошибка: грань %d — нет файла рядом с программой (%s/textures/"
          "%d.bmp) или в текущей папке (textures/, lab5/textures/). "
          "Форматы: 24/32 bpp RGB, 8 bpp RGB или RLE.\n",
          i + 1, exeDir, i + 1);
      img = makeFallbackTexture();
    }
    g_textures[i] = uploadTexture(img);
  }
}

static GLUquadric *sunQuadric() {
  static GLUquadric *q = nullptr;
  if (!q) {
    q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_FALSE);
  }
  return q;
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

static void drawLightBulbSphere(float lx, float ly, float lz) {
  glPushMatrix();
  glTranslatef(lx, ly, lz);
  glColor3f(1.0f, 0.95f, 0.55f);
  gluSphere(sunQuadric(), 0.07, 28, 18);
  glPopMatrix();
}

static void drawLightSource(float lx, float ly, float lz) {
  bool wasLit = glIsEnabled(GL_LIGHTING);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  drawLightBulbSphere(lx, ly, lz);
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

  float vlx = lx - cx, vly = ly - cy, vlz = lz - cz;
  float distLightSq = vlx * vlx + vly * vly + vlz * vlz;
  float distCubeCenterSq = g_camDist * g_camDist;
  bool sunBehindCube = distLightSq > distCubeCenterSq;

  if (g_transparent) {
    if (sunBehindCube) {
      glDisable(GL_LIGHTING);
      glDisable(GL_TEXTURE_2D);
      drawLightBulbSphere(lx, ly, lz);
      if (g_lightingOn) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
      }
    }

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

    glDisable(GL_TEXTURE_2D);
    drawCubeEdges(g_explode);

    if (sunBehindCube) {
      glDisable(GL_LIGHTING);
      glDisable(GL_TEXTURE_2D);
      glColor3f(0.4f, 0.4f, 0.25f);
      glBegin(GL_LINES);
      glVertex3f(0.f, 0.f, 0.f);
      glVertex3f(lx, ly, lz);
      glEnd();
      if (g_lightingOn) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
      }
    } else {
      drawLightSource(lx, ly, lz);
    }
  } else {
    drawCubeAllFaces(verts.data());
    glDisable(GL_TEXTURE_2D);
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
  printf(
      "Текстуры: каталог textures/ рядом с исполняемым файлом или в текущей "
      "директории (ещё lab5/textures/). Форматы BMP: 24/32 bpp, 8 bpp RGB/RLE. "
      "При ошибке — заглушка 1×1.\n");
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
  loadAllTextures(argv);

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
