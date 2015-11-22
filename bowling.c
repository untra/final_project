#include "bowling.h"



// bowling pins
void bowling_pin(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY)
{
  int resolution = 24;
  double fraction = 360 / resolution;
  double tilt,  width, alphawidth, height, additive;
  glPushMatrix();
  glTranslated(x, y,z);
  glRotated(rotateY, 0,1,0);
  glRotated(rotateX, 1,0,0);
  glScaled(xScale, yScale, zScale);
  int i;
  int k;
  double alphaheight = 0.0;
  double alphatilt = 0.0;
  double turning = 0.0;
  double f = 0.0;
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(1,1,1);
  glNormal3f( 0, -1, 0);
  glVertex3f(0,0, 0);
  //bottom
  for(i = 0; i <= resolution ; i++)
  {
    glVertex3f(Cos(fraction*i),0, Sin(fraction*i));
  }
  glEnd();
  //side 1
  for(k = 1; k <= 13 ; k++)
  {
    additive = k < 12 ? (double)((12 - k)*(12 - k))/300 : 0;
    tilt = alphatilt;
    alphatilt = Sin((k)*15+turning);
    width = 1 + tilt;
    alphawidth = 1 + alphatilt;
    height = alphaheight;
    alphaheight +=  0.5;
    alphaheight += additive;
    glBegin(GL_QUAD_STRIP);
    if((k == 13))
    {
      glColor3f(0.765,0.0,0.2);
    }
    else
    {
      glColor3f(1,1,1);
    }
    for(i = 0; i <= resolution ; i++)
    {
      f = (double)i / resolution;
      glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
      glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
      glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
    }
    glEnd();
  }
  //middle white stripe
  width = alphawidth;
  alphawidth = 0.7;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top red stripe
  width = alphawidth;
  alphawidth = 0.75;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(0.765,0.0,0.2);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.85;
  height = alphaheight;
  alphaheight += 1.0;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.75;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.60;
  height = alphaheight;
  alphaheight += 0.35;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.15;
  height = alphaheight;
  alphaheight += 0.2;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(1,1,1);
  glNormal3f( 0, 1, 0);
  glVertex3f(0,alphaheight, 0);
  //top
  for(i = 0; i <= resolution ; i++)
  {
    glVertex3f(alphawidth * Sin(fraction*i), alphaheight, alphawidth * Cos(fraction*i));
  }
  glEnd();
  glPopMatrix();
}

void alley(double x, double y, double z,
  double xScale, double yScale, double zScale,
  double rotateX, double rotateY, unsigned int floor_texture, unsigned int arrow_texture )
  {
    int i, l;
    double k,j;
    // int resolution  = 12;
    // double fraction = 360 / resolution;
    double length = 12;
    // double slightlyabove = 0.001;
    // double midway, xp, zp, prevx;

    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);


    glBegin(GL_QUAD_STRIP);
    glColor3f(0.6,0.6,0.6);
    for(l = 0; l < 2 ; l++)
    {
      for(i = 0; i < length ; i++)
      {
        k = l ==0 ? -0.2 : 1.2;
        glBegin(GL_QUAD_STRIP);
        glNormal3f( -1, 0, 0);
        glVertex3f(l,0,i);
        glVertex3f(l,0,i+1);
        glVertex3f(l,-0.1,i);
        glVertex3f(l,-0.1,i+1);
        glEnd();
        glBegin(GL_QUAD_STRIP);
        glNormal3f( 0, 1, 0);
        glVertex3f(l,-0.1,i);
        glVertex3f(l,-0.1,i+1);
        glVertex3f(k,-0.1,i);
        glVertex3f(k,-0.1,i+1);

        glEnd();
        glBegin(GL_QUAD_STRIP);
        glNormal3f( 1, 0, 0);
        glVertex3f(k,-0.1,i);
        glVertex3f(k,-0.1,i+1);
        glVertex3f(k,0,i);
        glVertex3f(k,0,i+1);
        glEnd();

      }
    }

    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glNormal3f( 0, 1, 0);
    for(i = 0; i < length ; i++)
    {
      k = (double)((double)i / length);
      j = (double)((double)(i+1) / length);
      glBindTexture(GL_TEXTURE_2D,floor_texture);
      glBegin(GL_QUAD_STRIP);
      glNormal3f( 0, 1, 0);
      glTexCoord2f(0,k); glVertex3f(0,0,i);
      glTexCoord2f(1,k); glVertex3f(1,0,i);
      glTexCoord2f(0,j); glVertex3f(0,0,i+1);
      glTexCoord2f(1,j); glVertex3f(1,0,i+1);
      glEnd();
    }
    glDisable(GL_TEXTURE_2D);


    glPopMatrix();
  }

  void mural(double x, double y, double z,
    double xScale, double yScale, double zScale,
    double rotateX, double rotateY, unsigned int mural_texture )
    {
      int length = 12;
      float shinyvec[1];
      float emission = 0.0;
      float white[] = {1,1,1,1};
      float Emission[]  = {0.0,0.0,0.01*emission,1.0};

      glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
      glMaterialfv(GL_FRONT,GL_SPECULAR,white);
      glMaterialfv(GL_FRONT,GL_EMISSION,Emission);

      glPushMatrix();
      glTranslated(x, y,z);
      glRotated(rotateY, 0,1,0);
      glRotated(rotateX, 1,0,0);
      glScaled(xScale, yScale, zScale);

      glColor3f(1,1,1);



      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
      glBindTexture(GL_TEXTURE_2D,mural_texture);
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.6,0.6,0.6);
      glNormal3f( 0, 0.8, -0.2);
      glTexCoord2f(0,0);  glVertex3f(2,1,length);
      glTexCoord2f(1,0);  glVertex3f(0,1,length);
      glTexCoord2f(1,1);  glVertex3f(0,2,length-0.2);
      glTexCoord2f(0,1);  glVertex3f(2,2,length-0.2);
      glEnd();
      glDisable(GL_TEXTURE_2D);


      glPopMatrix();
    }


  /*  Make a cube of unit length. Rotate, scale, and translate based on
  designated parameters. */
  void cube(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int texture)
  {
    float gray[] = {0.1,0.1,0.1,1.0};

    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);

    glColor3f(0.892,0.872,0.784);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,gray);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_QUADS);
    glNormal3d(-1.0,0.0,0.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(zScale/5,yScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();


    glBegin(GL_QUADS);
    glNormal3d(1.0,0.0,0.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(zScale/5,yScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glEnd();


    glBegin(GL_QUADS);
    glNormal3d(0.0,0.0,-1.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,yScale/6);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,0.0,1.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glTexCoord2f(xScale/5,yScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,1.0,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,zScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(0.0,zScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,-1.0,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glTexCoord2f(xScale/5,zScale/6);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,zScale/6);
    glVertex3d(-0.5, 0.0, 0.5);
    glEnd();

    glPopMatrix();
  }

  /*  Make a cube of unit length. Rotate, scale, and translate based on
  designated parameters. */
  void pyramid(double x, double y, double z, double xScale, double yScale, double zScale, double rotateAngle, unsigned int texture)
  {
    float gray[] = {0.1,0.1,0.1,1.0};

    glPushMatrix();
    glTranslated(x, y , z);
    glRotated(rotateAngle, 0,1,0);
    glScaled(xScale, yScale, zScale);

    glColor3f(0.892,0.872,0.784);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,gray);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_TRIANGLES);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(xScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);


    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(zScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);


    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(xScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(-0.5, 0.0, 0.5);


    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(zScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();

    glPopMatrix();
  }

  /*  Draw a white sphere */
  void sphere(double x, double y, double z, double xScale, double yScale, double zScale)
  {
    double az;
    double el;

    float white[] = {1.0,1.0,1.0,1.0};
    float black[] = {0.0,0.0,0.0,1.0};

    glPushMatrix();
    glTranslated(x, y, z);
    glScaled(xScale, yScale, zScale);
    glColor3f(1,1,1);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,white);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,black);
    for(el = -90.0; el < 90; el+=0.5)
    {
      glBegin(GL_QUAD_STRIP);
      for(az = 0.0; az <= 360.0; az+=0.5)
      {
        glNormal3d(Sin(az)*Cos(el),Sin(el),Cos(az)*Cos(el));
        glVertex3d(Sin(az)*Cos(el),Sin(el),Cos(az)*Cos(el));
        glNormal3d(Sin(az)*Cos(el+0.5),Sin(el+0.5),Cos(az)*Cos(el+0.5));
        glVertex3d(Sin(az)*Cos(el+0.5),Sin(el+0.5),Cos(az)*Cos(el+0.5));
      }
      glEnd();
    }

    glPopMatrix();
  }
