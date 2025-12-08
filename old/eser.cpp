#define GL_SILENCE_DEPRECATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GL/glew.h"
#include <GLUT/glut.h>

GLfloat r = 0.0;
GLfloat t = 0.0f; // inizializzato
GLfloat s = 0.0f; // inizializzato
GLboolean flag = false;

void printMatrixByRows(GLfloat *m );
void printMatrixByColumns(GLfloat *m );


void changeSize(int w, int h) {

    if(h == 0)
        h = 1;

    float ratio = 1.0 * w / h;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glViewport(0, 0, w, h); // Lower left corner of the viewport, in pixels, then w and h

    gluPerspective(45,ratio,1,1000); // To set fovy, aspect, i.e., w/h, zNear, zFar

    glMatrixMode(GL_MODELVIEW);
}


void renderScene1(void) {

    // Rendering of a simple, wireframe red cube (STATIC)
    
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    // Uncomment to rotate
    // glRotatef(45,0,0,1);
    
    glutWireCube(1);
    
    glutSwapBuffers();
}


void renderScene2(void) {

    // Rendering of a simple, wireframe red cube (ROTATED/ROTATING)

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    //glRotatef(45,0,0,1);
    glRotatef(r+=0.1,0,0,1); // Need to call glutIdleFunc() in main()
    glutWireCube(1);
    
    glutSwapBuffers();
}



void renderScene3(void) {

    // Observing MATRICES (and their changes) after ROTATION

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    if(flag==false){
        GLfloat mvm[16];
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);
        // Print GL_MODELVIEW_MATRIX
        printf("GL_MODELVIEW_MATRIX as initialized\n");
        printMatrixByRows(mvm);
        //printMatrixByColumns(mvm);

        glRotatef(45,0,0,1);
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);

        // Print GL_MODELVIEW_MATRIX after rotation
        printf("GL_MODELVIEW_MATRIX after rotation\n");
        printMatrixByRows(mvm);
        //printMatrixByColumns(mvm);

        flag=true;
    }
    
    glutWireCube(1);
    
    glutSwapBuffers();
}



void renderScene4(void) {

    // Observing MATRICES (and their changes) after TRANSLATION

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    if(flag==false){
        GLfloat mvm[16];
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);
        // Print GL_MODELVIEW_MATRIX
        printf("GL_MODELVIEW_MATRIX as initialized\n");
        printMatrixByColumns(mvm);

        glTranslatef(0.5,0,0);
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);

        // Print GL_MODELVIEW_MATRIX after rotation
        printf("GL_MODELVIEW_MATRIX after translation\n");
        printMatrixByColumns(mvm);

        flag=true;
    }
    
    glutWireCube(1);
    
    glutSwapBuffers();
}


void renderScene5(void) {

    // Preparing and using custom MATRICES (example with SCALING)

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    if(flag==false){
        GLfloat mvm[16];
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);
        // Print GL_MODELVIEW_MATRIX
        printf("GL_MODELVIEW_MATRIX as initialized\n");
        printMatrixByColumns(mvm);

        // glScalef(1.5,1.5,1.0);

        // With custom-prepared matrix
        
        float m[4][4] = {
            {1.3f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.3f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.3f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}
        };

        glLoadMatrixf((float*)m); // Load (apply) matrix

        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);
        
        // Print GL_MODELVIEW_MATRIX after scaling
        printf("GL_MODELVIEW_MATRIX after scaling\n");
        printMatrixByColumns(mvm);

        flag=true;
    }
    
    glutWireCube(1);

    glutSwapBuffers();
}

void renderScene6(void) {

    // Compose (multiply) matrices, and observe the impact of ORDER

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    if(flag==false){

        glTranslatef(0.5,0,0);
        glRotatef(45,0,0,1);

        GLfloat mvm[16];
        glGetFloatv(GL_MODELVIEW_MATRIX,mvm);
        // Print GL_MODELVIEW_MATRIX after composition
        printf("GL_MODELVIEW_MATRIX after composition\n");
        printMatrixByColumns(mvm);
    
        flag = true;
    }
    
    glutWireCube(1);

    glutSwapBuffers();
}


void renderScene7(void) {

    // Draw complex scenes by STACKING transformations

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    glColor3f(1.0, 0.0, 0.0);

    glPushMatrix(); // Save current matrix, pushing it in the stack

    glTranslatef(0.5,0,0); // Move where to draw parent element
    glScalef(0.3,1.0,1.0);
    glutWireCube(1);

    glColor3f(0.0, 0.0, 1.0);

    glPushMatrix(); // Save matrix, pushing it in the stack
    glTranslatef(0.8, 0.4, 0.0); // Move where to draw child element, relative to parent
    glScalef(0.1,0.3,1.0);
    glutWireCube(1);
    glPopMatrix(); // Restore matrix, popping it from the stack

    glPushMatrix(); // Save matrix, pushing it in the stack
    glTranslatef(0.8, -0.4, 0.0); // Move where to draw child element, relative to parent
    glScalef(0.1,0.3,1.0);
    glutWireCube(1);
    glPopMatrix(); // Restore matrix, popping it from the stack

    glPushMatrix(); // Save matrix, pushing it in the stack
    glTranslatef(-0.8, 0.4, 0.0); // Move where to draw child element, relative to parent
    glScalef(0.1,0.3,1.0);
    glutWireCube(1);
    glPopMatrix(); // Restore matrix, popping it from the stack

    glPushMatrix(); // Save matrix, pushing it in the stack
    glTranslatef(-0.8, -0.4, 0.0); // Move where to draw child element, relative to parent
    glScalef(0.1,0.3,1.0);
    glutWireCube(1);
    glPopMatrix(); // Restore matrix, popping it from the stack

    glPopMatrix(); // Restore matrix, popping it from the stack
    
    glutSwapBuffers();
}

void renderScene8(void) {

    // From orthographic view to perspective view
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
    
    // To set position of eye point, reference point, and direction of up vector
    
    gluLookAt(0.0,0.0,-10.0,
              0.0,0.0,0.0,
              0.0f,1.0f,0.0f);
    
    glColor3f(1.0, 0.0, 0.0);
    
    //glRotatef(r+=0.1,0.0,1.0,0.0);
    glutWireCube(1);
    
    glutSwapBuffers();
}


int main(int argc, char **argv) {
    glutInit(&argc, argv); // The OpenGL Utility Toolkit
    
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(320,320);
    glutCreateWindow("Esempio 1 - Trasformazioni");

    // se usi GLEW, inizializzalo subito DOPO la creazione del contesto (glutCreateWindow)
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        return 1;
    }

    glutDisplayFunc(renderScene2); // Change render scene (1 to 8)
    glutIdleFunc(renderScene2); // Un-comment for scenes wit h animations (2 and 8), and adjust argument accordingly
    //glutReshapeFunc(changeSize); // Un-comment only for scene 8 (for perspective proj.)
    
    glClearColor(1.0,1.0,1.0,1.0);

    //glewInit(); // OpenGL Extension Wrangler Library

    glutMainLoop();
    return 0;
}








void printMatrixByRows(GLfloat *m ){
    for(int i=0;i<16;i++){
        printf("\t%.2f ", m[i]);
        if(i%4==3)
            printf("\n");
    }
}

void printMatrixByColumns(GLfloat *m ){

    printf("\t%.2f ", m[0]);
    printf("\t%.2f ", m[4]);
    printf("\t%.2f ", m[8]);
    printf("\t%.2f ", m[12]);
    printf("\n");
    printf("\t%.2f ", m[1]);
    printf("\t%.2f ", m[5]);
    printf("\t%.2f ", m[9]);
    printf("\t%.2f ", m[13]);
    printf("\n");
    printf("\t%.2f ", m[2]);
    printf("\t%.2f ", m[6]);
    printf("\t%.2f ", m[10]);
    printf("\t%.2f ", m[14]);
    printf("\n");
    printf("\t%.2f ", m[3]);
    printf("\t%.2f ", m[7]);
    printf("\t%.2f ", m[11]);
    printf("\t%.2f ", m[15]);
    printf("\n");

}
