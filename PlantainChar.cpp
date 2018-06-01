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

#define SCREENCAP true
#define PORT 9400
#define FRAMES 3592
sf::RenderWindow * window;
float low_val, mid_val, high_val;
int screen_width = 1280;
int screen_height = 720;
int freq_vals[FRAMES][3];
bool program_done = false;
int frame;

class Cube {
private:
    sf::Vector3f position;
public:
    void set_position(sf::Vector3f new_position){
        position = new_position;
    }
    void draw(){
        glPushMatrix();
        
        double scale_val = 1.0 + (float)freq_vals[frame][2]*0.01;
        glScalef(scale_val,scale_val,scale_val);
        glTranslatef(position.x,position.y,position.z);
        glRotatef((float)freq_vals[frame][0]*0.25,0.0,0.0,1.0);
        glColor3f(1.0,1.0,1.0);
        glBegin(GL_QUADS);
        glVertex2f(-0.25,-0.25);
        glVertex2f(-0.25,0.25);
        glVertex2f(0.25,0.25);
        glVertex2f(0.25,-0.25);
        glEnd();
        glPopMatrix();
    }
};

Cube cubes[8];

void display(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    
    glLoadIdentity();
    //MODIFY THE CAMERA HERE
    
    gluPerspective(45.0,(float)screen_width/(float)screen_height,0.1,100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    //DISPLAY STUFF HERE
    glTranslatef(0.0,0.0,-10.0);
    
	for (int i = 0; i < 8; i++){
        cubes[i].draw();
    }
}

void update(){
    frame++;
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
    
    for (int i = 0; i < 8; i++){
        cubes[i].set_position(sf::Vector3f(((float)(i)-3.5)*0.6,0.0,0.0));
    }
    glClearColor(0.0,0.0,0.0,1.0);
	glViewport(0.0,0.0,1280,720);
    glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_EMISSION);
	glEnable(GL_COLOR_MATERIAL);
}

int main(int argc, char **argv) {

	window = new sf::RenderWindow(sf::VideoMode(1280,720),"Plantain Char");
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
    	Sleep(10);
#endif
    	window->display();
    }

	return 1;
}