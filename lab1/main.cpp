#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>

static float g_time = 0.f;
const float g_speed = 0.4f;

static void drawGround(float nightFactor) {
  float k = 1.f - nightFactor * 0.7f;
  float left = -1.8f, right = 1.8f;
  glBegin(GL_TRIANGLES);
  glColor3f(0.2f * k, 0.6f * k, 0.2f * k);
  glVertex2f(left, -1.f);
  glVertex2f(right, -1.f);
  glVertex2f(right, -0.3f);
  glVertex2f(left, -1.f);
  glVertex2f(right, -0.3f);
  glVertex2f(left, -0.3f);
  glEnd();
}

static void drawHouse(float nightFactor) {
  float k = 1.f - nightFactor * 0.7f;
  glBegin(GL_TRIANGLES);
  glColor3f(0.7f * k, 0.5f * k, 0.3f * k);
  glVertex2f(-0.5f, -0.3f);
  glVertex2f(0.1f, -0.3f);
  glVertex2f(0.1f, 0.2f);
  glVertex2f(-0.5f, -0.3f);
  glVertex2f(0.1f, 0.2f);
  glVertex2f(-0.5f, 0.2f);
  glColor3f(0.5f * k, 0.2f * k, 0.1f * k);
  glVertex2f(-0.55f, 0.2f);
  glVertex2f(0.15f, 0.2f);
  glVertex2f(-0.2f, 0.5f);
  glEnd();
}

static void drawTree(float x, float nightFactor) {
  float y = -0.3f;
  float k = 1.f - nightFactor * 0.7f;
  glBegin(GL_TRIANGLES);
  glColor3f(0.4f * k, 0.25f * k, 0.1f * k);
  glVertex2f(x - 0.03f, y);
  glVertex2f(x + 0.03f, y);
  glVertex2f(x + 0.03f, y + 0.2f);
  glVertex2f(x - 0.03f, y);
  glVertex2f(x + 0.03f, y + 0.2f);
  glVertex2f(x - 0.03f, y + 0.2f);
  glColor3f(0.1f * k, 0.45f * k, 0.1f * k);
  glVertex2f(x - 0.12f, y + 0.2f);
  glVertex2f(x + 0.12f, y + 0.2f);
  glVertex2f(x, y + 0.45f);
  glEnd();
}

static void drawSun(float cx, float cy, float r) {
  glColor3f(1.f, 0.95f, 0.7f);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  const int N = 32;
  for (int i = 0; i <= N; ++i) {
    float a = (float)i / (float)N * 6.283185307f;
    glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
  }
  glEnd();
}

static void drawMoon(float cx, float cy, float r) {
  glColor3f(0.88f, 0.9f, 1.f);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  const int N = 32;
  for (int i = 0; i <= N; ++i) {
    float a = (float)i / (float)N * 6.283185307f;
    glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
  }
  glEnd();
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

  drawGround(nightFactor);
  drawHouse(nightFactor);
  drawTree(-0.8f, nightFactor);
  drawTree(0.5f, nightFactor);
  drawTree(0.75f, nightFactor);
  if (night)
    drawMoon(cx, cy, 0.08f);
  else
    drawSun(cx, cy, 0.08f);

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
  glutCreateWindow("Lab1 - House, meadow, forest, sun/moon");
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutTimerFunc(32, timer, 0);
  glutMainLoop();
  return 0;
}
