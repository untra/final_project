/*
*  Textures
*
*  Draws 3 Barrells to demonstrate orthogonal & prespective projections
*  Throws in some lighting and sweet bowling_pin textures as well
*
*  Key bindings:
*  m          Toggle between perspective and orthogonal
*  +/-        Changes field of view for perspective
*  a          Toggle axes
*  arrows     Change view angle
*  PgDn/PgUp  Zoom in and out
*  0          Reset view angle
*  ESC        Exit
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "CSCIx229.h"
#include "bowling.h"

//  OpenGL with prototypes for glext
#define GL_GLxOffsetT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

int rollmax           = 600;
int explosion_limit   = 60;
int explode_at        = 540;

//Azimuth
int th=0;
//Elevation
int ph=0;

//Projection variables
int mode = 1;
int fov=60;
double asp=1;
double dim=50.0;

//first person location
double xOffset = 0;
double yOffset = 2.0;
double zOffset = -60.0;

int roll = 600;
int axes=0;       //  Display axes
double idle[6] = {0.0,0.0,0.0,0.0,0.0,0.0};
int window_width = 600;
int window_height = 600;
int left_click_down = 0;
int saved_th = 0;
int saved_ph = 0;
int light = 1;
int camera = 1;
int move = 1;       //  Move light
int one       =   1;  // Unit value
int distance  =   5;  // Light distance
int inc       =  10;  // Ball increment
int smooth    =   1;  // Smooth/Flat shading
int local     =   0;  // Local Viewer Model

unsigned int side = 0;// side texture
unsigned int end = 0; // end texture
unsigned int floor_texture = 0; // floor texture
unsigned int arrow_texture = 0; // floor texture
unsigned int mural_texture[4] = {0,0,0,0};
unsigned int wall_texture = 0; // wall texture
unsigned int ball_texture = 0; // bowling ball texture
unsigned int cieling_texture = 0; // bowling ball texture
unsigned int duct_texture = 0;

double AX = 0; // x-coordinate of where the camera is looking
double AY = 0; // y-coordinate of where the camera is looking
double AZ = 0; // z-coordinate of where the camera is looking

double UX = 0; // x-coordinate of the up vector
double UY = 1; // y-coordinate of the up vector
double UZ = 0; // z-coordinate of the up vector

int pole_position[8] = {0,0,0,0,0,0,0,0};
double explosion[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
double reset[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
double bowling_ball_z[8] = {-25.0,-25.0,-25.0,-25.0,-25.0,-25.0,-25.0,-25.0};
int ball_ph[8] = {0,0,0,0,0,0,0,0};
int pin_x[8] = {19, 0, -17, -36, -53, -72, -89, -108};
float colors[8][4] = {
{1.0, 0.1, 0.6 , 1.0},
{0.1, 1.0, 0.6 , 1.0},
{0.8, 1.0, 1.0 , 1.0},
{0.4, 0.7, 1.0 , 1.0},
{0.3, 1.0, 0.3 , 1.0},
{0.2, 0.3, 0.6 , 1.0},
{0.7, 0.4, 0.5 , 1.0},
{1.0, 1.0, 0.3 , 1.0}
};


/*
*  Convenience routine to output raster text
*  Use VARARGS to make this more flexible
*/
#define LEN 8192  //  Maximum length of text string
void Print(const char* format , ...)
{
  char    buf[LEN];
  char*   ch=buf;
  va_list args;
  //  Turn the parameters into a character string
  va_start(args,format);
  vsnprintf(buf,LEN,format,args);
  va_end(args);
  //  Display the characters one at a time at the current raster position
  while (*ch)
  glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*ch++);
}

void checkOffsets()
{
    if(xOffset > 29.5)
        xOffset = 29.5;
    else if(xOffset < -105.5)
        xOffset = -105.5;
    if(zOffset > 120)
        zOffset = 120;
    else if(zOffset < -60)
        zOffset = -60;
    if(yOffset > 28)
        yOffset = 28;
    else if(yOffset < 2)
        yOffset = 2;
}

double mouse_rotation(double delta, double mid)
{
  return 180 * (delta / mid);
}

/*
*  Draw vertex in polar coordinates with normal
*/
static void Vertex(double th,double ph)
{
  double x = Sin(th)*Cos(ph);
  double y = Cos(th)*Cos(ph);
  double z =         Sin(ph);
  //  For a sphere at the origin, the position
  //  and normal vectors are the same
  glNormal3d(x,y,z);
  glVertex3d(x,y,z);
}

static void ball(double x,double y,double z,double r, float color[])
 {
   int th,ph;
   float Emission[]  = {color[0],color[1],color[2],1.0};
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(r,r,r);
   //  White ball
   glColor3f(color[0],color[1],color[2]);
   float shinyvec[1] = {4};
   glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
   glMaterialfv(GL_FRONT,GL_SPECULAR,color);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   for (ph=-90;ph<90;ph+=inc)
   {
     glBegin(GL_QUAD_STRIP);
     for (th=0;th<=360;th+=2*inc)
     {
       Vertex(th,ph);
       Vertex(th,ph+inc);
     }
     glEnd();
   }
   //  Undo transofrmations
   glPopMatrix();
 }

 double bowling_ball_x(int lane)
 {
   int shift = lane % 2;
   if( bowling_ball_z[lane] > -20.0)
   {
     return -36.0 * (lane/2) + 24 + (!shift? 0: - 19.0);
   }
   else
   {
     return -36.0 * (lane/2) + 15 + (!shift? 0.25: -1.25);
   }
 }

 double bowling_ball_y(int lane)
 {
   if( bowling_ball_z[lane] > -5.0)
   {
     return 1;
   }
   if(bowling_ball_z[lane] < -9 && bowling_ball_z[lane] > -11)
   {
     return -3.0;
   }
   if( bowling_ball_z[lane] > 110)
   {
     return -0.5*(bowling_ball_z[lane]-110);
   }
   else
   {
     return 3.7;
   }
 }


  /*
  *  OpenGL (GLUT) calls this routine to display the scene
  */
  void display()
  {
    int i;
    double fall;
    const double len=10.0;  //  Length of axes
    //  Erase the window and the depth buffer
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    //  Enable Z-buffering in OpenGL
    glEnable(GL_DEPTH_TEST);
    //  Undo previous transformations
    glLoadIdentity();

    if(mode == 0)
    {
        //Set up perspective projection
        double Ex = 2*dim*Sin(th)*Cos(ph);
        double Ey = 2*dim*Sin(ph);
        double Ez = -2*dim*Cos(th)*Cos(ph);
        gluLookAt(Ex,Ey,Ez ,0,0,0, 0,Cos(ph),0);
    }
    else
    {
        //Set up first person projection
        double Cx = -2*dim*Sin(th)*Cos(ph) + xOffset;
        double Cy = 2*dim*Sin(ph) + yOffset;
        double Cz = 2*dim*Cos(th)*Cos(ph) + zOffset;
        gluLookAt(xOffset,yOffset,zOffset ,Cx,Cy,Cz, 0,Cos(ph),0);
    }

    //  Light switch
    if (light)
    {
      //  Translate intensity to color vectors
      float Ambient[]   = {0.1,0.1,0.1,1.0};
      float Diffuse[]   = {0.3,0.3,0.3,1};
      float Specular[]  = {0.2,0.2,0.2,1};
      float white[]     = {1,1,1,1};
      // float red[]     = {1,0,0,1};
      // float green[]     = {0,1,0,1};
      // float blue[]     = {0,0,1,1};
      //  Light direction
      float Position[6][4]  = {
        {68*Cos(idle[0])-38,30,20 , 1},
        {68*Cos(idle[1])-38,31,40 , 1},
        {68*Cos(idle[2])-38,32,60 , 1},
        {68*Cos(idle[3])-38,33,80 , 1},
        {68*Cos(idle[4])-38,34,100 , 1},
        {bowling_ball_x(4),bowling_ball_y(4),bowling_ball_z[4] , 1},
      };
      // printf("%f | %f | %f\n" , Position[0] , Position[1] , Position[2]    );
      //  Draw light position as ball (still no lighting here)
      ball(Position[0][0],Position[0][1],Position[0][2],1,white);
      // ball(Position[1][0],Position[1][1],Position[1][2],1,red);
      // ball(Position[2][0],Position[2][1],Position[2][2],1,green);
      // ball(Position[3][0],Position[3][1],Position[3][2],1,blue);
      // ball(Position[4][0],Position[4][1],Position[4][2],1,white);

      //  Enable lighting
      glEnable(GL_LIGHTING);
      //  Enable lighting with normalization
      glEnable(GL_NORMALIZE);

      //  Location of viewer for specular calculations
      glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,local);

      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      glEnable(GL_COLOR_MATERIAL);
      //  Enable light 0

      //  Set ambient, diffuse, specular components and position of light 0
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
      glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
      glLightfv(GL_LIGHT0,GL_SPECULAR,Specular);
      glLightfv(GL_LIGHT0,GL_POSITION,Position[0]);
      glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glEnable(GL_LIGHT1);
      // glLightfv(GL_LIGHT1,GL_AMBIENT ,Ambient);
      // glLightfv(GL_LIGHT1,GL_DIFFUSE ,Diffuse);
      // glLightfv(GL_LIGHT1,GL_SPECULAR,Specular);
      // glLightfv(GL_LIGHT1,GL_POSITION,Position[1]);
      glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      //  Set ambient, diffuse, specular components and position of light 1

      // glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,red);
      // glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      // //  Set ambient, diffuse, specular components and position of light 2
      // glEnable(GL_LIGHT2);
      // glLightfv(GL_LIGHT2,GL_AMBIENT ,Ambient);
      // glLightfv(GL_LIGHT2,GL_DIFFUSE ,Diffuse);
      // glLightfv(GL_LIGHT2,GL_SPECULAR,Specular);
      // glLightfv(GL_LIGHT2,GL_POSITION,Position[2]);
      // glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,green);
      // glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      // //  Set ambient, diffuse, specular components and position of light 3
      // glEnable(GL_LIGHT3);
      // glLightfv(GL_LIGHT3,GL_AMBIENT ,Ambient);
      // glLightfv(GL_LIGHT3,GL_DIFFUSE ,Diffuse);
      // glLightfv(GL_LIGHT3,GL_SPECULAR,Specular);
      // glLightfv(GL_LIGHT3,GL_POSITION,Position[3]);
      // glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,blue);
      // glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      // //  Set ambient, diffuse, specular components and position of light 4
      // glEnable(GL_LIGHT4);
      // glLightfv(GL_LIGHT4,GL_AMBIENT ,Ambient);
      // glLightfv(GL_LIGHT4,GL_DIFFUSE ,Diffuse);
      // glLightfv(GL_LIGHT4,GL_SPECULAR,Specular);
      // glLightfv(GL_LIGHT4,GL_POSITION,Position[4]);
      // glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
      // glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      // //  Set ambient, diffuse, specular components and position of light 5
      // glEnable(GL_LIGHT5);
      // glLightfv(GL_LIGHT5,GL_AMBIENT ,Ambient);
      // glLightfv(GL_LIGHT5,GL_DIFFUSE ,Diffuse);
      // glLightfv(GL_LIGHT5,GL_SPECULAR,Specular);
      // glLightfv(GL_LIGHT5,GL_POSITION,Position[5]);
      // glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      // glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,colors[4]);
      // glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


    wall(33.5,0,-30,10,12,10,0,180,wall_texture);
    wall(33.5,0,0,10,12,10,0,180,wall_texture);
    wall(33.5,0,30,10,12,10,0,180,wall_texture);
    wall(33.5,0,60,10,12,10,0,180,wall_texture);
    wall(33.5,0,90,10,12,10,0,180,wall_texture);
    wall(33.5,0,120,10,12,10,0,180,wall_texture);
    mural(-2,0,-10, 12,12,10 , 0, 0, mural_texture[0], light);
    mural(-38,0,-10, 12,12,10 , 0, 0, mural_texture[1], light);
    mural(-74,0,-10, 12,12,10 , 0, 0, mural_texture[2], light);
    mural(-110,0,-10, 12,12,10 , 0, 0, mural_texture[3], light);
    for(i = 0 ; i < 8 ; i++)
    {
      fall = explosion[i] > 180 ? (double)explosion[i]/180.0 : 0;
      fall = fall*fall;
      pins(pin_x[i],reset[i] - fall,0,1,1,1,0,explosion[i]);
    }
    double_lane(0,0,0,1,1,1,0,0,floor_texture, cieling_texture, duct_texture);
    double_lane(-36,0,0,1,1,1,0,0,floor_texture, cieling_texture, duct_texture);
    double_lane(-72,0,0,1,1,1,0,0,floor_texture, cieling_texture, duct_texture);
    double_lane(-108,0,0,1,1,1,0,0,floor_texture, cieling_texture, duct_texture);
    wall(-110.5,0,-60,10,12,10,0,0,wall_texture);
    wall(-110.5,0,-30,10,12,10,0,0,wall_texture);
    wall(-110.5,0,0,10,12,10,0,0,wall_texture);
    wall(-110.5,0,30,10,12,10,0,0,wall_texture);
    wall(-110.5,0,60,10,12,10,0,0,wall_texture);
    wall(-110.5,0,90,10,12,10,0,0,wall_texture);
    for(i = 0 ; i < 8 ; i++)
    {
      bowling_ball(bowling_ball_x(i),bowling_ball_y(i),bowling_ball_z[i],1,1,1,ball_ph[i],ball_texture, colors[i]);
    }

    //enable textures


    //  disbale lighting from here on
    if (light)
    {
      glDisable(GL_LIGHTING);
    }
    //  Draw axes
    glColor3f(1,1,1);
    if (axes)
    {
      glBegin(GL_LINES);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(len,0.0,0.0);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(0.0,len,0.0);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(0.0,0.0,len);
      glEnd();
      //  Label axes
      glRasterPos3d(len,0.0,0.0);
      Print("X");
      glRasterPos3d(0.0,len,0.0);
      Print("Y");
      glRasterPos3d(0.0,0.0,len);
      Print("Z");
    }

      glWindowPos2i(5,5);
      if(mode == 0)
      {
          Print("Projection Type: Perspective");
          glWindowPos2i(5,25);
          Print("Dimension: %.0f", dim);
      }
      else
      {
          Print("Projection Type: First Person");
          glWindowPos2i(5,25);
          Print("User Location: { %.3f, %.3f, %.3f }", xOffset, yOffset, zOffset);
      }

      //Render scene
      glFlush();
      glutSwapBuffers();
  }

  /*
  *  GLUT calls this routine when an arrow key is pressed
  */
  void special(int key,int x,int y)
  {
    //  Right arrow key - increase angle by 5 degrees
    if (key == GLUT_KEY_RIGHT)
    th += 5;
    //  Left arrow key - decrease angle by 5 degrees
    else if (key == GLUT_KEY_LEFT)
    th -= 5;
    //  Up arrow key - increase elevation by 5 degrees
    else if (key == GLUT_KEY_UP)
    ph += 5;
    //  Down arrow key - decrease elevation by 5 degrees
    else if (key == GLUT_KEY_DOWN)
    ph -= 5;
    //  PageUp key - increase dim
    else if (key == GLUT_KEY_PAGE_UP)
    dim += 0.1;
    //  PageDown key - decrease dim
    else if (key == GLUT_KEY_PAGE_DOWN && dim>1)
    dim -= 0.1;
    //  Keep angles to +/-360 degrees
    th %= 360;
    ph %= 360;
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  void gobowl(int lane)
  {
    if(bowling_ball_z[lane] > -28.0)
    {
      return;
    }
    else
    {
      bowling_ball_z[lane] = -2.0;
    }
  }

  /*
  *  GLUT calls this routine when a key is pressed
  */
  void key(unsigned char ch,int x,int y)
  {
    //  Exit on ESC
    if (ch == 27)
    exit(0);
    //  Reset view angle
    else if (ch == '0')
    {
      th = ph = 0;
      xOffset = 0;
      yOffset = 2.0;
      zOffset = -60.0;
    }
    else if (ch == '1')
    {
      gobowl(0);
    }
    else if (ch == '2')
    {
      gobowl(1);
    }
    else if (ch == '3')
    {
      gobowl(2);
    }
    else if (ch == '4')
    {
      gobowl(3);
    }
    else if (ch == '5')
    {
      gobowl(4);
    }
    else if (ch == '6')
    {
      gobowl(5);
    }
    else if (ch == '7')
    {
      gobowl(6);
    }
    else if (ch == '8')
    {
      gobowl(7);
    }
    //  Toggle axes
    else if (ch == 'x' || ch == 'X')
    axes = 1-axes;
    //  Toggle lighting
    else if (ch == 'l' || ch == 'L')
    light = 1-light;
    //  Switch projection mode
    else if (ch == 'p' || ch == 'P')
    {
      mode = (mode+1)%3;
      th = ph = 0;
      xOffset = 0;
      yOffset = 0;
      zOffset = 2*dim;
    }
    //  Change field of view angle
    else if (ch == '-' && ch>1)
    fov--;
    else if (ch == '+' && ch<179)
    fov++;
    else if (ch == 'r')
    {
      roll = 0;
      xOffset = -31.0;
      yOffset = 4.0;
      zOffset = 0.0;
      th = 0;
      ph  = 180;
    }
    //FIRST PERSON NAVIGATION WITH WASD
    else if(ch == 'w' || ch == 'W')
    {
        xOffset -= 2*Sin(th);
        zOffset += 2*Cos(th);
        checkOffsets();
    }
    else if(ch == 's' || ch == 'S')
    {
        xOffset += 2*Sin(th);
        zOffset -= 2*Cos(th);
        checkOffsets();
    }
    else if(ch == 'a' || ch == 'A')
    {
        xOffset -= 2*Sin(th-90);
        zOffset += 2*Cos(th-90);
        checkOffsets();
    }
    else if(ch == 'd' || ch == 'D')
    {
        xOffset += 2*Sin(th-90);
        zOffset -= 2*Cos(th-90);
        checkOffsets();
    }
    else if(ch == '[')
    {
        yOffset += 1;
        checkOffsets();
    }
    else if(ch == ']')
    {
        yOffset -= 1;
        checkOffsets();
    }

    //  Reproject
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  void mouse(int button, int state, int x, int y)
  {
    // Wheel reports as button 3(scroll up) and button 4(scroll down)
    if ((button == 3) || (button == 4)) // It's a wheel event
    {
      if (button == 3)
      dim += 0.1;
      else if(dim>1)
      dim -= 0.1;
    }
    if ((button == 0) && (state == GLUT_DOWN || state == GLUT_UP))
    {
      if (state == GLUT_DOWN)
      left_click_down = 1;
      else if (state == GLUT_UP)
      left_click_down = 0;
      double midx = (window_width / 2);
      double midy = (window_height / 2);
      double deltax = (midx - x);
      double deltay = (midy - y);
      saved_th = th - mouse_rotation(deltax , midx);
      saved_ph = ph - mouse_rotation(deltay , midy);
    }
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  void motionmouse(int x, int y)
  {
    if (left_click_down)
    {
      double midx = (window_width / 2);
      double midy = (window_height / 2);
      double deltax = (midx - x);
      double deltay = (midy - y);
      th = mouse_rotation(deltax , midx) + saved_th;
      ph = mouse_rotation(deltay , midy) + saved_ph;
      th %= 360;
      ph %= 360;
    }
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }



  /*
  *  GLUT calls this routine when the window is resized
  */
  void reshape(int width,int height)
  {
    window_width = width;
    window_height = height;
    //  Ratio of the width to the height of the window
    asp = (height>0) ? (double)width/height : 1;
    //  Set the viewport to the entire window
    glViewport(0,0, width,height);
    //  Set projection
    Project(fov,asp,dim);
  }

  void explode_and_reset(int lane)
  {
    double explosion_change = 12.0;
    if (explosion[lane] > 0.1)
    {
      if(explosion[lane] < 360.0)
      {
        explosion[lane] += explosion_change;
      }
      else
      {
        explosion[lane] = 0.0;
        reset[lane] = 30.0;
      }
    }
    if(reset[lane] > 0.0)
    {
      reset[lane] -= 0.1;
    }
  }

  void roll_dat_ball(int lane)
  {
    if(bowling_ball_z[lane] > -3.0)
    {
      bowling_ball_z[lane] += 0.8;
      ball_ph[lane] += 32;
      ball_ph[lane] %= 360;
    }
    if(bowling_ball_z[lane] > 105.0)
    {
      explosion[lane] += 1.0;
    }
    if(bowling_ball_z[lane] > 110.0)
    {
      bowling_ball_z[lane] = -10.0;
    }
    if(bowling_ball_z[lane] < -9 && bowling_ball_z[lane] > -11)
    {
      if(reset[lane] < 1.0 && explosion[lane] < 0.1)
      {
        bowling_ball_z[lane] = -20;
      }
    }
    if(bowling_ball_z[lane] < -19.5 && bowling_ball_z[lane] > -28.9)
    {
      bowling_ball_z[lane] -= 0.3;
      ball_ph[lane] -= 12;
      ball_ph[lane] %= 360;
    }
  }

  /*
  *  GLUT calls this toutine when there is nothing else to do
  */
  void idlefunc()
  {
    double t = glutGet(GLUT_ELAPSED_TIME)/1000.0;
    int change = 2.0;
    int lane;
    idle[0] = fmod(90*t,360);
    idle[1] = fmod(45*t,360);
    idle[2] = fmod(30*t,360);
    idle[3] = fmod(22.5*t,360);
    idle[4] = fmod(18*t,360);
    idle[5] = fmod(15*t,360);
    //If rolling
    if(roll < rollmax)
    {
      roll += 1*change;
      ph -= (((6.0*change)*360) / rollmax) ;
      ph %= 360;
      zOffset += ((change*120.0) / rollmax);
      yOffset = 3 - 1.5*Sin(ph);
      explosion[3] += zOffset > 105.0 ? 1.0 : 0.0 ;
      if(zOffset >= 120.0)
      {
        ph = 0;
        zOffset = 0;
        yOffset = 3;
        xOffset = -31.0;
        roll = rollmax;
      }
    }
    for(lane = 0; lane < 8 ; lane++)
    {
      explode_and_reset(lane);
      roll_dat_ball(lane);
    }
    glutPostRedisplay();
  }



  /*
  *  Start up GLUT and tell it what to do
  */
  int main(int argc,char* argv[])
  {
    //  Initialize GLUT
    glutInit(&argc,argv);
    //  Request double buffered, true color window with Z buffering at 600x600
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
    glutInitWindowSize(window_width,window_height);
    glutCreateWindow("Samuel Volin");

    glutIdleFunc(idlefunc);
    //  Set callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motionmouse);
    glutKeyboardFunc(key);
    // load textures
    floor_texture = LoadTexBMP("floor.bmp");
    arrow_texture = LoadTexBMP("arrows.bmp");
    mural_texture[0] = LoadTexBMP("1.bmp");
    mural_texture[1] = LoadTexBMP("2.bmp");
    mural_texture[2] = LoadTexBMP("3.bmp");
    mural_texture[3] = LoadTexBMP("4.bmp");
    wall_texture = LoadTexBMP("wall.bmp");
    ball_texture = LoadTexBMP("ball.bmp");
    cieling_texture = LoadTexBMP("cieling.bmp");
    duct_texture = LoadTexBMP("duct.bmp");
    glutMainLoop();
    return 0;
  }
