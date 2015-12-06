#ifndef bowling
#define bowling

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "CSCIx229.h"

#ifdef USEGLEW
#include <GL/glew.h>
#endif
#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void cieling(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int cieling_texture);

void duct_cube(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int duct_texture);

void bowling_pin(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY);

void alley(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int floor_texture);

void mural(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int mural_texture, int lighting );

void divider(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY );

void upcurve(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY );

void ball_return_body(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY );

void cap(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY );

void floor_panel(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int floor_texture );

void wall(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int floor_texture );

void double_lane(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double explosion, unsigned int floor_texture, unsigned int cieling_texture, unsigned int duct_texture);

void pins(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double explosion);

void bowling_ball(double x, double y, double z, double xScale, double yScale, double zScale, double rotation, unsigned int ball_texture, float color[]);

void cube(double x, double y, double z, double xScale, double yScale, double zScale, double rotateY, double rotateZ, unsigned int texture);

void pyramid(double x, double y, double z, double xScale, double yScale, double zScale, double rotateAngle, unsigned int texture);


#ifdef __cplusplus
}
#endif

#endif
