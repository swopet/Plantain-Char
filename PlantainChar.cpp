#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <Winsock2.h>
#include <string.h>

#define SCREENCAP false
#define PORT 9400
#define FRAMES 3592
sf::RenderWindow * window;
float low_val, mid_val, high_val;
double cam_rotate_speed = 0.5;
double cam_move_speed = 0.05;
double cam_pos = -12.0;
double cam_rotation = 0.0;
int screen_width = 1280;
int screen_height = 720;
int freq_vals[FRAMES][3];
bool program_done = false;
int frame;

GLfloat cube_top_face[] = {0.0,1.0,0.0,-0.5,0.5,-0.5,0.0,1.0,0.0,-0.5,0.5,0.5,0.0,1.0,0.0,0.5,0.5,0.5,0.0,1.0,0.0,0.5,0.5,-0.5};
GLuint cube_indices[] = {0,1,2,3};

class Cube {
private:
    sf::Vector3f position;
    double rotate_val = 0.0;
    double rotate_speed = 0.0;
    double scale_val = 0.0;
    double distance = 0.0;
public:
    void set_position(sf::Vector3f new_position){
        position = new_position;
    }
    void draw(){
        glPushMatrix();
        
        glLineWidth(2.0);
        glTranslatef(position.x,position.y,position.z);
        double my_scale_val = (1.0 + scale_val*0.01);
        if (distance <= 0.5){
            glPopMatrix();
            return;
        }
        else {
            my_scale_val = my_scale_val*0.1*(distance-0.5);
            std::cerr << distance << std::endl;
        }
        glScalef(my_scale_val,my_scale_val,my_scale_val);
        glRotatef(rotate_val,1.0,1.0,1.0);
        glInterleavedArrays(GL_N3F_V3F,0,cube_top_face);
        for (int j = 0; j < 2; j++){
            glPushMatrix();
            if (j==0) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glColor3f(0.1,0.1,0.1);
                glScalef(1.01,1.01,1.01);
            }
            else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glColor3f(1.0,1.0,1.0);
            }
            for (int i = 0; i < 6; i++){
                glPushMatrix();
                if (i==1){
                    glRotatef(180.0,1.0,0.0,0.0);
                }
                else if (i!=0){
                    glRotatef(90.0*i,0.0,1.0,0.0);
                    glRotatef(90.0,1.0,0.0,0.0);   
                }
                glDrawElements(GL_QUADS,4,GL_UNSIGNED_INT,cube_indices);
                glPopMatrix();
            }
            glPopMatrix();
        }   
        glPopMatrix();
    }
    void update(){
        sf::Vector3f distance_vec = position - sf::Vector3f(0.0,cam_pos,0.0);
        distance = sqrt(distance_vec.x*distance_vec.x+distance_vec.y*distance_vec.y+distance_vec.z*distance_vec.z);
        scale_val = scale_val * 0.7 + (float)freq_vals[frame][2]*0.3;
        rotate_speed = rotate_speed * 0.2 + (float)freq_vals[frame][0]*0.8;
        rotate_val += 3.0 - rotate_speed * 0.25;
    }
};

Cube cubes[512];

void display(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    
    glLoadIdentity();
    gluPerspective(45.0,(float)screen_width/(float)screen_height,0.1,100);
    
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    //EDIT LIGHTING HERE
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 45.0);
    GLfloat spot_direction[] = {0.0, 0.0, -1.0 };
    GLfloat light_position[] = {0.0, 0.0, 0.0, 1.0};
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, spot_direction);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glEnable(GL_LIGHT0);
    
    //MODIFY THE CAMERA HERE
    glRotatef(cam_rotation,0.0,1.0,0.0);
    glTranslatef(0.0,-cam_pos,0.0);
    
    
    
    //DISPLAY STUFF HERE
    
	for (int i = 0; i < 512; i++){
        cubes[i].draw();
    }
}

void update(){
    frame++;
    for (int i = 0; i < 512; i++){
        cubes[i].update();
    }
    cam_pos += cam_move_speed;
    cam_rotation += cam_rotate_speed;
}

void init(){
	std::ifstream f("vals.txt");
    std::string in;
    int curr_val = 0;
    for (int i = 0; i < FRAMES; i++){
        f >> in;
        unsigned int ind = 0;
        for (int j = 0; j < 3; j++){
            while (true){
                if (ind == in.length() || in[ind] == ','){
                    freq_vals[i][j] = curr_val;
                    curr_val = 0;
                    ind++;
                    break;
                }
                else{
                    curr_val = curr_val * 10;
                    curr_val += (in[ind]-'0');
                    ind++;
                }
                
            }
            std::cout << freq_vals[i][j] << "\t";
        }
        std::cout << std::endl;
    }
    
    for (int i = 0; i < 512; i++){
        cubes[i].set_position(sf::Vector3f(((float)(i%8)-3.5)*2.0,((float)((i/8)%8)-3.5)*2.0,((float)(i/64)-3.5)*2.0));
    }
    glClearColor(0.0,0.0,0.0,1.0);
	glViewport(0.0,0.0,1280,720);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CCW);
    glShadeModel (GL_SMOOTH);
	
	glEnable(GL_EMISSION);
	glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
	
	glEnable(GL_NORMALIZE);
}

int main(int argc, char **argv) {
    sf::ContextSettings settings;
	settings.depthBits = 24;
	settings.stencilBits = 0;
	settings.antialiasingLevel = 8;
	settings.majorVersion = 3;
	settings.minorVersion = 0;
	window = new sf::RenderWindow(sf::VideoMode(1280,720),"Plantain Char",sf::Style::Default, settings);
#ifdef SCREENCAP
	int screenshot_count = 0;
#endif
    window->setVerticalSyncEnabled(true);

    // activate the window
    window->setActive(true);
    init();

    bool running = true;
    while (running){
        // handle events
        sf::Event event;
        while (window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                // end the program
                running = false;
                window->close();
                program_done = true;
                Sleep(10);
                exit(0);
            }
            else if (event.type == sf::Event::Resized)
            {
                
            }
        }
        update();
    	display();
#if SCREENCAP == true
        sf::Vector2u windowSize = window->getSize();
        sf::Texture new_tex;
        new_tex.create(windowSize.x,windowSize.y);
        new_tex.update(*window);
        sf::Image screenshot = new_tex.copyToImage();
        std::stringstream file_name;
        file_name << "screenshots/frame_" << screenshot_count << ".png";
        if (screenshot.saveToFile(file_name.str())){
            screenshot_count++;
        }
#else
    	Sleep(50);
#endif
    	window->display();
    }

	return 1;
}