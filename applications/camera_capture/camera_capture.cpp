/*
Copyright (c) 2013-2015, Gregory P. Meyer
                         University of Illinois Board of Trustees
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder(s) nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Standard Libraries
#include <stdio.h>
#include <stdlib.h>

// OpenGL
#include <GL/glut.h>

// DIP
#include <dip/cameras/primesense.h>
#include <dip/cameras/softkinetic.h>
#include <dip/common/types.h>
#include <dip/io/hdf5dumper.h>
#include <dip/io/hdf5wrapper.h>
#include <dip/visualization/colorize.h>

using namespace dip;

const int kWindowWidth = 640;
const int kWindowHeight = 480;

const int kFramesPerSecond = 60;

const int kMinDepth = 64;
const int kMaxDepth = 8192;

Camera *g_camera = NULL;
HDF5Wrapper *g_dump = NULL;
HDF5Dumper *g_dumper = NULL;

Depth *g_depth = NULL;
Color *g_colorized_depth = NULL;
Color *g_color = NULL;

GLuint g_texture;
int g_display = DEPTH_SENSOR;

void close() {
  if (g_camera != NULL)
    delete g_camera;
  if (g_dumper != NULL)
    delete g_dumper;
  if (g_dump != NULL)
    delete g_dump;

  if (g_depth != NULL)
    delete [] g_depth;
  if (g_colorized_depth != NULL)
    delete [] g_colorized_depth;
  if (g_color != NULL)
    delete [] g_color;

  exit(0);
}

void display() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0.0f, 1.0f, 0.0f, 1.0f, -10.0f, 10.0f);

  // Update depth image.
  if (g_camera->Update(g_depth)) {
    printf("Unable to update depth image.\n");
    close();
  }

  // Update color image.
  if (g_camera->Update(g_color)) {
    printf("Unable to update color image.\n");
    close();
  }

  // Update Dump File
  static int count = 0;

  char group[64];
  sprintf(group, "/FRAME%04d", count);

  hsize_t dimensions[3];

  // Dump depth image.
  dimensions[0] = (hsize_t)g_camera->height(DEPTH_SENSOR);
  dimensions[1] = (hsize_t)g_camera->width(DEPTH_SENSOR);

  g_dumper->Write("DEPTH", group, g_depth, sizeof(Depth) *
                  g_camera->width(DEPTH_SENSOR) *
                  g_camera->height(DEPTH_SENSOR),
                  dimensions, 2, H5T_NATIVE_SHORT);

  // Dump color image.
  dimensions[0] = (hsize_t)g_camera->height(COLOR_SENSOR);
  dimensions[1] = (hsize_t)g_camera->width(COLOR_SENSOR);
  dimensions[2] = 3;

  g_dumper->Write("COLOR", group, g_color, sizeof(Color) *
                  g_camera->width(COLOR_SENSOR) *
                  g_camera->height(COLOR_SENSOR),
                  dimensions, 3, H5T_NATIVE_UCHAR);

  count++;

  if (g_display == DEPTH_SENSOR) {
    // Colorize Depth
    static Colorize colorize;
    colorize.Run(g_camera->width(DEPTH_SENSOR), g_camera->height(DEPTH_SENSOR),
                 kMinDepth, kMaxDepth, g_depth, g_colorized_depth);

    // Update Texture
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, g_texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_camera->width(DEPTH_SENSOR),
                    g_camera->height(DEPTH_SENSOR), GL_RGB, GL_UNSIGNED_BYTE,
                    g_colorized_depth);

    glDisable(GL_TEXTURE_2D);
  }
  else {
    // Update Texture
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, g_texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_camera->width(COLOR_SENSOR),
                    g_camera->height(COLOR_SENSOR), GL_RGB, GL_UNSIGNED_BYTE,
                    g_color);

    glDisable(GL_TEXTURE_2D);
  }

  // Display Frame
  glEnable(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, g_texture);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 0.0f, 0.0f);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glutSwapBuffers();
}

void reshape(int w, int h) {
  glViewport(0, 0, w, h);
}

void keyboard(unsigned char key, int x, int y) {
  switch (key) {
  // Quit Program
  case 27:
    close();
    break;
  case '1':
    g_display = DEPTH_SENSOR;
    break;
  case '2':
    g_display = COLOR_SENSOR;
    break;
  }
}

void timer(int fps) {
  glutPostRedisplay();
  glutTimerFunc(1000 / fps, timer, fps);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: %s <Dump File> <Compression Level>\n", argv[0]);
    return -1;
  }

  glutInit(&argc, argv);

  // Initialize camera.
#ifndef SOFTKINETIC
  g_camera = new PrimeSense();
#else
  g_camera = new SoftKinetic();
#endif

  if (!g_camera->enabled()) {
    printf("Unable to Open Camera\n");
    return -1;
  }

  // Initialize dump file.
  g_dump = new HDF5Wrapper(argv[1], CREATE_HDF5);
  g_dump->Compression(atoi(argv[2]));

  // Initialize dumper.
  g_dumper = new HDF5Dumper(g_dump);

  if (!g_dump->enabled()) {
    printf("Unable to Create Dump File\n");
    return -1;
  }

  // Output dimensions of depth and color images.
  int width, height;

  width = g_camera->width(DEPTH_SENSOR);
  height = g_camera->height(DEPTH_SENSOR);

  g_dump->Write("WIDTH", "/INFORMATION/DEPTH_SENSOR",
                &width, H5T_NATIVE_INT);
  g_dump->Write("HEIGHT", "/INFORMATION/DEPTH_SENSOR",
                &height, H5T_NATIVE_INT);

  width = g_camera->width(COLOR_SENSOR);
  height = g_camera->height(COLOR_SENSOR);

  g_dump->Write("WIDTH", "/INFORMATION/COLOR_SENSOR",
                &width, H5T_NATIVE_INT);
  g_dump->Write("HEIGHT", "/INFORMATION/COLOR_SENSOR",
                &height, H5T_NATIVE_INT);

  // Output focal length of depth and color images.
  float fx, fy;

  fx = g_camera->fx(DEPTH_SENSOR);
  fy = g_camera->fy(DEPTH_SENSOR);

  g_dump->Write("FX", "/INFORMATION/DEPTH_SENSOR",
                &fx, H5T_NATIVE_FLOAT);
  g_dump->Write("FY", "/INFORMATION/DEPTH_SENSOR",
                &fy, H5T_NATIVE_FLOAT);

  fx = g_camera->fx(COLOR_SENSOR);
  fy = g_camera->fy(COLOR_SENSOR);

  g_dump->Write("FX", "/INFORMATION/COLOR_SENSOR",
                &fx, H5T_NATIVE_FLOAT);
  g_dump->Write("FY", "/INFORMATION/COLOR_SENSOR",
                &fy, H5T_NATIVE_FLOAT);

  // Initialize Buffers
  g_depth = new Depth[g_camera->width(DEPTH_SENSOR) *
                      g_camera->height(DEPTH_SENSOR)];
  g_colorized_depth = new Color[g_camera->width(DEPTH_SENSOR) *
                                g_camera->height(DEPTH_SENSOR)];
  g_color = new Color[g_camera->width(COLOR_SENSOR) *
                      g_camera->height(COLOR_SENSOR)];

  // Initialize OpenGL
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(kWindowWidth, kWindowHeight);
  glutInitWindowPosition(100, 100);
  glutCreateWindow("Camera Capture");

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutTimerFunc(1000 / kFramesPerSecond, timer, kFramesPerSecond);

  // Initialize Texture
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_texture);
  glBindTexture(GL_TEXTURE_2D, g_texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
               g_camera->width(COLOR_SENSOR) > g_camera->width(DEPTH_SENSOR) ?
               g_camera->width(COLOR_SENSOR) : g_camera->width(DEPTH_SENSOR),
               g_camera->height(COLOR_SENSOR) > g_camera->height(DEPTH_SENSOR) ?
               g_camera->height(COLOR_SENSOR) : g_camera->height(DEPTH_SENSOR),
               0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

  glDisable(GL_TEXTURE_2D);

  glutMainLoop();

  return 0;
}
