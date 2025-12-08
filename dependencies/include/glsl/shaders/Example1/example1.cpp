#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glut.h>

GLuint v,f,p;
GLfloat r;
GLfloat deltar = 0.0;

void changeSize(int w, int h) {

	if(h == 0)
		h = 1;

	float ratio = 1.0* w / h;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

    glViewport(0, 0, w, h);

	gluPerspective(45,ratio,1,1000);
	glMatrixMode(GL_MODELVIEW);
}

void renderScene(void) {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();
	gluLookAt(0.0,0.0,5.0, 
		      0.0,0.0,-1.0,
			  0.0f,1.0f,0.0f);
	glColor3f(1.0, 0.0, 0.0);
	glPushMatrix();	
	glRotatef(r,0.0,1.0,0.0);
	r+=deltar;
	glutSolidTeapot(1);
	glPopMatrix();

	glutSwapBuffers();
}

char *textFileRead(char *fn) {
	FILE *fp;
	char *content = NULL;
	int count=0;
	if (fn != NULL) {
		fp = fopen(fn,"rt");
		if (fp != NULL) {
	      fseek(fp, 0, SEEK_END);
	      count = ftell(fp);
	      rewind(fp);
			if (count > 0) {
				content = (char *)malloc(sizeof(char) * (count+1));
				count = fread(content,sizeof(char),count,fp);
				content[count] = '\0';
			}
			fclose(fp);
		}
	}
	return content;
}

void setShaders() {
	char *vs,*fs;

	v = glCreateShader(GL_VERTEX_SHADER); 
	f = glCreateShader(GL_FRAGMENT_SHADER);	

	vs = textFileRead("../diffuse.vert"); 
	fs = textFileRead("../diffuse.frag");

	const char * vv = vs; 
	const char * ff = fs;

	glShaderSource(v, 1, &vv,NULL); 
	glShaderSource(f, 1, &ff,NULL);

	free(vs); 
	free(fs);

	glCompileShader(v); 
	glCompileShader(f);

	p = glCreateProgram();

	glAttachShader(p,v); 
	glAttachShader(p,f);

	glLinkProgram(p);
	glUseProgram(p);
}


int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(320,320);
	glutCreateWindow("Example 1");

	glutDisplayFunc(renderScene);
	glutIdleFunc(renderScene);
	glutReshapeFunc(changeSize);

	glEnable(GL_DEPTH_TEST);
	glClearColor(1.0,1.0,1.0,1.0);

	glewInit();

	setShaders();

	glutMainLoop();
	return 0;
}