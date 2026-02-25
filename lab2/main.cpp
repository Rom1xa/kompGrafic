#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <vector>

static float g_time = 0.f;
const float g_speed = 0.4f;
const int FLOATS_PER_VERTEX = 5;

static void buildSceneArrays(std::vector<float> &verts,
                             std::vector<int> &counts, float nightFactor) {
  verts.clear();
  counts.clear();
  float k = 1.f - nightFactor * 0.7f;

  auto pushTri = [&](float x1, float y1, float r1, float g1, float b1, float x2,
                     float y2, float r2, float g2, float b2, float x3, float y3,
                     float r3, float g3, float b3) {
    verts.push_back(x1);
    verts.push_back(y1);
    verts.push_back(r1 * k);
    verts.push_back(g1 * k);
    verts.push_back(b1 * k);
    verts.push_back(x2);
    verts.push_back(y2);
    verts.push_back(r2 * k);
    verts.push_back(g2 * k);
    verts.push_back(b2 * k);
    verts.push_back(x3);
    verts.push_back(y3);
    verts.push_back(r3 * k);
    verts.push_back(g3 * k);
    verts.push_back(b3 * k);
    counts.push_back(3);
  };

  // Земля
  float left = -1.8f, right = 1.8f;
  pushTri(left, -1, 0.2f, 0.6f, 0.2f, right, -1, 0.2f, 0.6f, 0.2f, right, -0.3f,
          0.25f, 0.65f, 0.25f);
  pushTri(left, -1, 0.2f, 0.6f, 0.2f, right, -0.3f, 0.25f, 0.65f, 0.25f, left,
          -0.3f, 0.25f, 0.65f, 0.25f);
  // Дом: стена
  pushTri(-0.5f, -0.3f, 0.7f, 0.5f, 0.3f, 0.1f, -0.3f, 0.7f, 0.5f, 0.3f, 0.1f,
          0.2f, 0.7f, 0.5f, 0.3f);
  pushTri(-0.5f, -0.3f, 0.7f, 0.5f, 0.3f, 0.1f, 0.2f, 0.7f, 0.5f, 0.3f, -0.5f,
          0.2f, 0.7f, 0.5f, 0.3f);
  // Крыша
  pushTri(-0.55f, 0.2f, 0.5f, 0.2f, 0.1f, 0.15f, 0.2f, 0.5f, 0.2f, 0.1f, -0.2f,
          0.5f, 0.6f, 0.25f, 0.12f);
  // Деревья
  float treeY = -0.3f;
  float treeX[] = {-0.8f, 0.5f, 0.75f};
  for (float x : treeX) {
    pushTri(x - 0.03f, treeY, 0.4f, 0.25f, 0.1f, x + 0.03f, treeY, 0.4f, 0.25f,
            0.1f, x + 0.03f, treeY + 0.2f, 0.45f, 0.28f, 0.12f);
    pushTri(x - 0.03f, treeY, 0.4f, 0.25f, 0.1f, x + 0.03f, treeY + 0.2f, 0.45f,
            0.28f, 0.12f, x - 0.03f, treeY + 0.2f, 0.45f, 0.28f, 0.12f);
    pushTri(x - 0.12f, treeY + 0.2f, 0.1f, 0.45f, 0.1f, x + 0.12f, treeY + 0.2f,
            0.1f, 0.45f, 0.1f, x, treeY + 0.45f, 0.12f, 0.5f, 0.12f);
  }
}

static void buildSunArray(std::vector<float> &verts, float cx, float cy,
                          float r) {
  verts.clear();
  float cr = 1.f, cg = 0.95f, cb = 0.7f;
  verts.push_back(cx);
  verts.push_back(cy);
  verts.push_back(cr);
  verts.push_back(cg);
  verts.push_back(cb);
  const int N = 32;
  for (int i = 0; i <= N; ++i) {
    float a = (float)i / (float)N * 6.283185307f;
    verts.push_back(cx + r * cosf(a));
    verts.push_back(cy + r * sinf(a));
    verts.push_back(cr);
    verts.push_back(cg);
    verts.push_back(cb);
  }
}

static void buildMoonArray(std::vector<float> &verts, float cx, float cy,
                           float r) {
  verts.clear();
  float cr = 0.88f, cg = 0.9f, cb = 1.f;
  verts.push_back(cx);
  verts.push_back(cy);
  verts.push_back(cr);
  verts.push_back(cg);
  verts.push_back(cb);
  const int N = 32;
  for (int i = 0; i <= N; ++i) {
    float a = (float)i / (float)N * 6.283185307f;
    verts.push_back(cx + r * cosf(a));
    verts.push_back(cy + r * sinf(a));
    verts.push_back(cr);
    verts.push_back(cg);
    verts.push_back(cb);
  }
}

static void drawFromArrays(const float *verts, int vertexCount) {
  if (vertexCount <= 0)
    return;
  glVertexPointer(2, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), verts);
  glColorPointer(3, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), verts + 2);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

static void drawSunMoonFromArray(const float *verts, int vertexCount) {
  if (vertexCount <= 0)
    return;
  glVertexPointer(2, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), verts);
  glColorPointer(3, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), verts + 2);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

static void display() {
  bool night = (g_time >= 1.f);
  float phase = night ? g_time - 1.f : g_time;
  float angle = phase * 3.14159265f;
  float cx = -cosf(angle) * 0.85f;
  float cy = sinf(angle) * 0.85f;
  float nightFactor = night ? 1.f : 0.f;

  glClearColor(0.53f - nightFactor * 0.4f, 0.81f - nightFactor * 0.5f,
               0.92f - nightFactor * 0.6f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  std::vector<float> sceneVerts;
  std::vector<int> sceneCounts;
  buildSceneArrays(sceneVerts, sceneCounts, nightFactor);
  int offset = 0;
  for (int count : sceneCounts) {
    drawFromArrays(sceneVerts.data() + offset * FLOATS_PER_VERTEX, count);
    offset += count;
  }

  float r = 0.08f;
  std::vector<float> circleVerts;
  if (night)
    buildMoonArray(circleVerts, cx, cy, r);
  else
    buildSunArray(circleVerts, cx, cy, r);
  drawSunMoonFromArray(circleVerts.data(),
                       (int)(circleVerts.size() / FLOATS_PER_VERTEX));

  glutSwapBuffers();
}

static void timer(int) {
  g_time += g_speed * 0.032f;
  if (g_time > 2.f)
    g_time -= 2.f;
  glutPostRedisplay();
  glutTimerFunc(32, timer, 0);
}

static void reshape(int w, int h) {
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = (float)w / (float)(h > 0 ? h : 1);
  if (aspect >= 1.f)
    glOrtho(-aspect, aspect, -1.f, 1.f, -1.f, 1.f);
  else
    glOrtho(-1.f, 1.f, -1.f / aspect, 1.f / aspect, -1.f, 1.f);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

int main(int argc, char **argv) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(800, 600);
  glutCreateWindow("Lab2 - Vertex arrays (массивы точек)");
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutTimerFunc(32, timer, 0);
  glutMainLoop();
  return 0;
}
