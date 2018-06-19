#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <Winsock2.h>
#include <string.h>

#define SCREENCAP false
#define PORT 9400
#define FRAMES 3592
#define SQRT_3 1.73205080757
#define SQRT_2 1.41421356237
sf::RenderWindow * window;
float low_val, mid_val, high_val;
double cam_rotate_speed = 0.5;
double cam_move_speed = 0.01;
sf::Vector3f cam_pos(0,0,0);
double cam_rotation = 0.0;
int screen_width = 1280;
int screen_height = 720;
double freq_vals[FRAMES][3];
bool program_done = false;
int frame;
sf::Texture floor_texture;

GLfloat cube_top_face[] = {0.0,1.0,0.0,-0.5,0.5,-0.5,0.0,1.0,0.0,-0.5,0.5,0.5,0.0,1.0,0.0,0.5,0.5,0.5,0.0,1.0,0.0,0.5,0.5,-0.5};
GLuint cube_indices[] = {0,1,2,3};

void multiply_vector(GLfloat * vec, GLfloat * matrix){
	sf::Vector3f buffer;
	buffer.x = vec[0]*matrix[0] + vec[1]*matrix[4] + vec[2]*matrix[8] + 1*matrix[12];
	buffer.y = vec[0]*matrix[1] + vec[1]*matrix[5] + vec[2]*matrix[9] + 1*matrix[13];
	buffer.z = vec[0]*matrix[2] + vec[1]*matrix[6] + vec[2]*matrix[10] + 1*matrix[14];
	//printf("quaternion: {\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f}\n",matrix[0],matrix[1],matrix[2],matrix[3],matrix[4],matrix[5],matrix[6],matrix[7],matrix[8],matrix[9],matrix[10],matrix[11],matrix[12],matrix[13],matrix[14],matrix[15]);
	//printf("previous point: (%f,%f,%f), ",vec[0],vec[1],vec[2]);
	vec[0]=buffer.x;
	vec[1]=buffer.y;
	vec[2]=buffer.z;
	//printf("transformed point: (%f,%f,%f)\n",vec[0],vec[1],vec[2]);
}

void draw_1x1_cube(bool outline, GLfloat color[]){
    glInterleavedArrays(GL_N3F_V3F,0,cube_top_face);
    glEnable(GL_COLOR_MATERIAL);
    for (int j = 0; j < 1 || (outline && j < 2); j++){
        glPushMatrix();
        if (j==1) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.1,0.1,0.1);
            glScalef(1.01,1.01,1.01);
        }
        else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor3f(color[0],color[1],color[2]);
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
}

enum CubeStage {
    BURSTING,
    FLOATING,
    ASSIGNED
};

std::vector<std::pair<int,int>> get_spiral_cubes(float offset, float direction = 1.0){
    std::vector<std::pair<int,int>> coords;
    int layers = 4;
    float period = 24.0;
    float t= 0.0;
    float last_t = 0.0;
    float increment = 2.0;
    int last_x = 0;
    int last_y = 0;
    int new_x;
    int new_y;
    coords.push_back(std::pair<int,int>(0,0));
    while (t < layers*period){
        t += increment;
        new_x = round( (t*12.0/period) * cos(direction*(t-offset)*M_PI*2.0/period));
        new_y = round( (t*12.0/period) * sin(direction*(t-offset)*M_PI*2.0/period));
        if (abs(new_x-last_x) > 1 || abs(new_y-last_y) > 1){
            t = last_t;
            increment = increment/2.0;
        }
        else{ //add rotation about X or about Z to get 3d spirals
            if (new_x != last_x || new_y != last_y){
                coords.push_back(std::pair<int,int>(new_x,new_y));
            }
            last_t = t;
            last_x = new_x;
            last_y = new_y;
        }
    }
    return coords;
}

void draw_cube_vector_2(std::vector<std::pair<int,int>> spiral, bool rotate = false){
    for (std::vector<std::pair<int,int>>::iterator iter = spiral.begin(); iter != spiral.end(); iter++){
        glPushMatrix();
        GLfloat color[] = {1.0,1.0,1.0};
        glTranslatef((*iter).first,(*iter).second,0.0);
        //glScalef(0.8,0.8,0.8);
        draw_1x1_cube(true,color);
        glPopMatrix();
    }
}

class KaleidofloorTile {
private:
    int divisions = 6;
    double radius = 4.0;
    double burst_level = 0.0;
    int num_triangles;
    int num_vertices;
    GLfloat * base_vertices; //T2F_N3F_V3F
    GLfloat * curr_vertices; //T2F_N3F_V3F
    GLuint * mesh_indices; //List of trios with the indices of triangle vertices
public:
    
    GLuint get_midpoint(std::map<long int, GLuint> * midpoints, int ind_1, int ind_2, int current_vert_ind, GLfloat * vertices){
		int low_ind = (ind_1 < ind_2) ? ind_1 : ind_2;
		int high_ind = (ind_1 < ind_2) ? ind_2 : ind_1;
		long int key = ((long unsigned int)low_ind << 16) + (long unsigned int)high_ind;
		int return_ind;
		try {
			return_ind = midpoints->at(key);
		}
		catch (const std::out_of_range& oor){
            vertices[current_vert_ind*8] = (vertices[ind_1*8]+vertices[ind_2*8])/2.0;
            vertices[current_vert_ind*8+1] = (vertices[ind_1*8+1]+vertices[ind_2*8+1])/2.0;
			float new_x = (vertices[ind_1*8+5] + vertices[ind_2*8+5])/2.0;
			float new_y = (vertices[ind_1*8+6] + vertices[ind_2*8+6])/2.0;
			float new_z = (vertices[ind_1*8+7] + vertices[ind_2*8+7])/2.0;
            vertices[current_vert_ind*8+2] = 0.0;
            vertices[current_vert_ind*8+3] = 1.0;
            vertices[current_vert_ind*8+4] = 0.0;
			vertices[current_vert_ind*8+5] = new_x;
			vertices[current_vert_ind*8+6] = new_y;
			vertices[current_vert_ind*8+7] = new_z;
			midpoints->insert(std::pair<long int,GLuint>(key,current_vert_ind));
			return current_vert_ind;
		}
		return return_ind;
	}
    void init(int new_divisions = 4, double new_radius = 6.0, int recursion_level = 4){
        divisions = new_divisions;
        radius = new_radius;
        num_vertices = (pow(2,recursion_level)+1)*(pow(2,recursion_level)+2)/2;
        num_triangles = pow(4, recursion_level);
        printf("Predicted Triangles: %d\n",num_triangles);
        printf("Predicted Vertices: %d\n",num_vertices);
        GLfloat * vertices = (GLfloat *)malloc(8*num_vertices*sizeof(GLfloat));
        GLuint * indices = (GLuint *)malloc(3*num_triangles*sizeof(GLuint));
        //point 0
        vertices[0] = 0.0;
        vertices[1] = 1.0;
        vertices[2] = 0.0;
        vertices[3] = 1.0;
        vertices[4] = 0.0;
        vertices[5] = 0.0;
        vertices[6] = 0.0;
        vertices[7] = 0.0;
        //point 1
        vertices[8] = 0.5;
        vertices[9] = 1.0;
        vertices[10] = 0.0;
        vertices[11] = 1.0;
        vertices[12] = 0.0;
        vertices[13] = radius*cos(M_PI/(double)divisions);
        vertices[14] = 0.0;
        vertices[15] = 0.0;
        //point 2
        vertices[16] = 0.5;
        vertices[17] = 1.0-0.5*sin(M_PI/(double)divisions);
        vertices[18] = 0.0;
        vertices[19] = 1.0;
        vertices[20] = 0.0;
        vertices[21] = radius*cos(M_PI/(double)divisions);
        vertices[22] = 0.0;
        vertices[23] = radius*sin(M_PI/(double)divisions);
        
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        
        //SUBDIVIDE HERE
        std::map<long int, GLuint> midpoints;
        int curr_num_triangles = 1;
        int current_tri_ind = 1;
        int current_vert_ind = 3;
        for (int i = 0; i < recursion_level; i++){
            curr_num_triangles = current_tri_ind;
            for (int j = 0; j < curr_num_triangles; j++){
                GLuint p0 = indices[j*3];
                GLuint p1 = indices[j*3+1];
                GLuint p2 = indices[j*3+2];
                GLuint p3 = get_midpoint(&midpoints,p0,p1,current_vert_ind,vertices);
                if (p3 == current_vert_ind) current_vert_ind++;
                GLuint p4 = get_midpoint(&midpoints,p1,p2,current_vert_ind,vertices);
                if (p4 == current_vert_ind) current_vert_ind++;
                GLuint p5 = get_midpoint(&midpoints,p2,p0,current_vert_ind,vertices);
                if (p5 == current_vert_ind) current_vert_ind++;
                indices[j*3] = p0;
                indices[j*3+1] = p3;
                indices[j*3+2] = p5;
                indices[current_tri_ind*3] = p3;
                indices[current_tri_ind*3+1] = p4;
                indices[current_tri_ind*3+2] = p5;
                current_tri_ind++;
                indices[current_tri_ind*3] = p3;
                indices[current_tri_ind*3+1] = p1;
                indices[current_tri_ind*3+2] = p4;
                current_tri_ind++;
                indices[current_tri_ind*3] = p5;
                indices[current_tri_ind*3+1] = p4;
                indices[current_tri_ind*3+2] = p2;
                current_tri_ind++;
            }
        }
        printf("Triangles: %d\n",current_tri_ind);
        printf("Vertices: %d\n",current_vert_ind);
        base_vertices = vertices;
        curr_vertices = (GLfloat *)malloc(8*num_vertices*sizeof(GLfloat));
        for (int i = 0; i < num_vertices; i++){
            for (int j = 0; j < 8; j++){
                curr_vertices[i*8+j] = base_vertices[i*8+j];
            }
        }
        mesh_indices = indices;
    }
    void draw(){
        
        glEnable(GL_TEXTURE_2D);
        sf::Texture::bind(&floor_texture);
        glInterleavedArrays(GL_T2F_N3F_V3F,0,curr_vertices);
        for (int i = 0; i < divisions; i++){
            glPushMatrix();
            glRotatef(i*360.0/divisions,0.0,1.0,0.0);
            glDrawElements(GL_TRIANGLES,num_triangles*3,GL_UNSIGNED_INT,mesh_indices);
            glScalef(1.0,1.0,-1.0);
            glDrawElements(GL_TRIANGLES,num_triangles*3,GL_UNSIGNED_INT,mesh_indices);
            glPopMatrix();
        }
        
        sf::Texture::bind(NULL);
        glDisable(GL_TEXTURE_2D);
    }
    void setBurstLevel(double new_burst){
        burst_level = new_burst;
    }
    void update(){
        for (int i = 0; i < num_vertices; i++){
            for (int j = 2; j < 8; j++){
                curr_vertices[i*8+j] = base_vertices[i*8+j];
            }
            if (burst_level > 0.0){
                sf::Vector3f diff_vec(base_vertices[i*8+5],base_vertices[i*8+6],base_vertices[i*8+7]);
                double diff = sqrt(diff_vec.x*diff_vec.x+diff_vec.y*diff_vec.y+diff_vec.z*diff_vec.z);
                double rot_val = 2.0*burst_level - diff;
                if (rot_val > 0.0){
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(0.0,burst_level*cos(diff*M_PI/4.0),0.0);
                    glTranslatef(2.0,-2.0,0.0);
                    glRotatef(-30.0*rot_val,0.0,0.0,1.0);
                    glTranslatef(-2.0,2.0,0.0);
                    GLfloat curr_matrix[16];
                    glGetFloatv(GL_MODELVIEW_MATRIX,curr_matrix);
                    multiply_vector(&curr_vertices[i*8+5],curr_matrix);
                    multiply_vector(&curr_vertices[i*8+2],curr_matrix);
                    glPopMatrix();
                }
            }
            
            curr_vertices[i*8] = base_vertices[i*8] + 0.5 + 0.5*cos((double)frame*M_PI/140.0);
            curr_vertices[i*8+1] = base_vertices[i*8+1] - 0.4 + 0.4*sin((double)frame*M_PI/70.0);
        }
        burst_level = burst_level - 0.1;
        //correct normals
    }
};

class HexFloor {
private:
    int num_layers;
    double radius = 4.0;
    std::map<std::pair<int,int>,KaleidofloorTile *> ** layers;
public:
    void init(int new_num_layers = 5){
        num_layers = new_num_layers;
        layers = (std::map<std::pair<int,int>,KaleidofloorTile *> **)malloc(num_layers*sizeof(std::map<std::pair<int,int>,KaleidofloorTile *> *));
        for (int i = 0; i < num_layers; i++){
            layers[i] = new std::map<std::pair<int,int>,KaleidofloorTile *>();
        }
        for (int x = -num_layers+1; x < num_layers; x++){
            for (int y = -num_layers+1; y < num_layers; y++){
                int z = -(x+y);
                //get max
                int layer_num = abs(x) >= abs(y) ? (abs(x) >= abs(z) ? abs(x) : abs(z)) : (abs(y) >= abs(z) ? abs(y) : abs(z));
                if (layer_num < num_layers){
                    KaleidofloorTile * new_tile = new KaleidofloorTile();
                    new_tile->init(6,radius,4);
                    //assign the new tile to the coordinate in the appropriate layer
                    (*layers[layer_num])[std::pair<int,int>(x,y)] = new_tile;
                }
            }
        }
    }
    void update(){
        
    }
    void draw(){
        for (int i = 0; i < num_layers; i++){
            for (std::map<std::pair<int,int>,KaleidofloorTile *>::iterator iter = layers[i]->begin(); iter != layers[i]->end(); iter++){
                double x = (double)((*iter).first.first);
                double z = (double)((*iter).first.second);
                glPushMatrix();
                //fancy math, don't question it. Trust me, these are the right coordinates yo.
                glTranslatef((x+z/2.0)*radius*SQRT_3,0.0,z*3.0*radius/2.0);
                (*iter).second->draw();
                glPopMatrix();
            }
        }
    }
};

class Kaleidofloor {
private:
    std::map<std::pair<int,int>,KaleidofloorTile *> tiles;
    std::vector<std::pair<int,int>> spirals[6];
    std::vector<std::pair<int,int>>::iterator iters[6];
    double stage = 0.0;
public:
    void init(){
        for (int x = -12; x <= 12; x++){
            for (int z = -12; z <= 12; z++){
                tiles[std::pair<int,int>(x,z)] = new KaleidofloorTile();
                tiles[std::pair<int,int>(x,z)]->init();
            }
        }
        for (int i = 0; i < 3; i++){
            spirals[i] = get_spiral_cubes(i*8.0);
        }
        for (int i = 0; i < 3; i++){
            spirals[3+i] = get_spiral_cubes(i*8.0,-1.0);
        }
        for (int i = 0; i < 6; i++){
            iters[i] = spirals[i].begin();
        }
    }
    void update(){
        for (int i = 0; i < 6; i++){
            if (iters[i] == spirals[i].end()){
                iters[i] = spirals[i].begin();
            }
            tiles[(*iters[i])]->setBurstLevel(stage);
        }
        for (std::map<std::pair<int,int>,KaleidofloorTile *>::iterator iter = tiles.begin(); iter != tiles.end(); iter++){
            (*iter).second->update();
        }
        if (stage == 1.0){
            stage = 0.0;
            for (int i = 0; i < 6; i++){
                iters[i]++;
            }
        }
        stage = stage + 0.2;
    }
    void draw(){
        for (std::map<std::pair<int,int>,KaleidofloorTile *>::iterator iter = tiles.begin(); iter != tiles.end(); iter++){
            glPushMatrix();
            glTranslatef((float)((*iter).first.first)*8.0,0.0,(float)((*iter).first.second)*8.0);
            (*iter).second->draw();
            glPopMatrix();
        }
    }
};

HexFloor hex_floor;
Kaleidofloor kaleido_floor;

class CubeController {
private:
    sf::Vector3f target_pos;
    sf::Vector3f start_pos;
    int start_frame;
    int end_frame;
    int layer;
public:
    CubeController(){
        target_pos = sf::Vector3f(0,0,0);
        start_pos = sf::Vector3f(0,0,0);
        start_frame = 0;
        end_frame = 1;
    }
    CubeController(sf::Vector3f new_target_pos){
        target_pos = new_target_pos;
        start_pos = sf::Vector3f(new_target_pos.x,-10.0,new_target_pos.z);
        start_frame = 0;
        end_frame = 100;
    }
    void set_frames(int new_layer){
        layer = new_layer;
        start_frame = layer*14 + rand()%14;
        end_frame = layer*14+140 + rand()%14;
    }
    void draw(){
        if (frame < start_frame){
            
        }
        else if (frame < end_frame){
            float ratio = (float)(frame-start_frame)/(float)(end_frame-start_frame);
            float wave_ratio = 0.5 - 0.5 * cos(ratio*M_PI);
            sf::Vector3f pos;
            pos = start_pos + ratio*(target_pos-start_pos);
            glPushMatrix();
            glRotatef(wave_ratio*720.0*(layer%2 ? -1.0 : 1.0),0.0,1.0,0.0);
            glTranslatef(pos.x*wave_ratio,pos.y,pos.z*wave_ratio);
            glTranslatef(wave_ratio*0.5*cos((target_pos.z*3.5+frame)*M_PI/28.0),wave_ratio*0.5*sin((sqrt(target_pos.x*target_pos.x+target_pos.z*target_pos.z)+frame+target_pos.y)*M_PI/28.0),0.0);
            
            glScalef(0.4+ratio*0.2,0.4+ratio*0.2,0.4+ratio*0.2);
            double dist = sqrt(target_pos.x*target_pos.x+target_pos.y*target_pos.y+target_pos.z*target_pos.z);
            double dist_modified = (target_pos.x+dist)*14.0 + frame;
            double dist_val = 1.2*wave_ratio*sin(dist_modified*M_PI/28.0)-0.2;
            if (dist_val > 0.0){
                glRotatef(dist_val*360.0,target_pos.x,target_pos.y,target_pos.z);
            }
            GLfloat color[] = {0.5+0.5*dist_val,1.0-abs(dist_val),1.0};
            draw_1x1_cube(true,color);
            glPopMatrix();
        }
        else{
            glPushMatrix();
            glTranslatef(target_pos.x,target_pos.y,target_pos.z);
            glTranslatef(0.5*cos((target_pos.z*3.5+frame)*M_PI/28.0),0.5*sin((sqrt(target_pos.x*target_pos.x+target_pos.z*target_pos.z)+frame+target_pos.y)*M_PI/28.0),0.0);
            
            glScalef(0.6,0.6,0.6);
            double dist = sqrt(target_pos.x*target_pos.x+target_pos.y*target_pos.y+target_pos.z*target_pos.z);
            double dist_modified = (target_pos.x+dist)*14.0 + frame;
            double dist_val = 1.2*sin(dist_modified*M_PI/28.0)-0.2;
            if (dist_val > 0.0){
                glRotatef(dist_val*360.0,target_pos.x,target_pos.y,target_pos.z);
            }
            GLfloat color[] = {0.5+0.5*dist_val,1.0-abs(dist_val),1.0};
            draw_1x1_cube(true,color);
            glPopMatrix();
        }
    }
};

std::vector<CubeController *> cubes;

void draw_cubes(){
    for (std::vector<CubeController *>::iterator iter = cubes.begin(); iter != cubes.end(); iter++){
        (*iter)->draw();
    }
}

void assign_cubes(double radius = 7.5){
    std::vector<CubeController *> layer;
    int layer_num = 0;
    int rad = (int)radius;
    for (int y = rad; y >= -rad; y--){
        for (int z = -rad; z <= rad; z++){
            for (int x = -rad; x <= rad; x++){
                double dist = sqrt(x*x+y*y+z*z);
                if (dist <= radius){
                    CubeController * new_cube = new CubeController(sf::Vector3f(x,y,z));
                    layer.push_back(new_cube);
                    new_cube->set_frames(layer_num);
                }
            }
        }
        std::random_shuffle (layer.begin(), layer.end() );
        while (!layer.empty()){
            cubes.push_back(layer.back());
            layer.pop_back();
        }
        layer_num++;
    }
}

double rot_val=0.0;
void draw_cube_sphere(double radius){
    rot_val += freq_vals[frame][2]/6.0;
    int rad = (int)radius;
    for (int x = -rad; x <= rad; x++){
        for (int y = rad; y <= -rad; y++){
            for (int z = -rad; z <= rad; z++){
                double dist = sqrt(pow((double)x,2.0)+pow((double)y,2.0)+pow((double)z,2.0));
                if (dist <= radius){
                    glPushMatrix();
                    glTranslatef(x,y,z);
                    GLfloat scale = 0.7+(0.2)*sin((double)(dist+(double)frame/14.0)*M_PI/(radius));
                    glScalef(scale,scale,scale);
                    sf::Vector3f cam_dist_vec(x-cam_pos.x,y-cam_pos.y,z-(cam_pos.z+24.0));
                    double cam_dist = sqrt(cam_dist_vec.x*cam_dist_vec.x+cam_dist_vec.y*cam_dist_vec.y+cam_dist_vec.z*cam_dist_vec.z);
                    if (cam_dist < 4.0){
                        double extra_scale = (cam_dist/4.0)*(cam_dist/4.0);//sin(cam_dist*M_PI/8.0);
                        glScalef(extra_scale,extra_scale,extra_scale);
                    }
                    if (frame < 240){
                        
                    }
                    else if (frame < 380)
                        glRotatef((double)(frame-240)*140.0/360.0*(x%2==0 ? 1.0 : -1.0),1.0,0.0,0.0);
                    else 
                        glRotatef(((double)(frame)-380.0)*(y%2==0 ? 1.0 : -1.0),0.0,1.0,0.0);
                    glRotatef(rot_val,x,y,z);
                    GLfloat color[] = {0.6+0.4*sin((double)(dist+(double)(frame+z*7.0)/14.0)*M_PI/(radius)),0.7,0.6+0.4*cos((double)(dist+(double)(frame+x*7.0)/14.0)*M_PI/(radius))};
                    draw_1x1_cube(true,color);
                    glPopMatrix();
                }
            }
        }
    }
}

/*class Cube {
private:
    sf::Vector3f;
    double rotate_val = 0.0;
    double rotate_speed = 0.0;
    double scale_val = 0.0;
    double distance = 0.0;
    sf::Vector3f rot_axis;
public:
    void set_position(sf::Vector3f new_position){
        position = new_position;
    }
    void draw(){
        glPushMatrix();
        
        glLineWidth(2.0);
        glTranslatef(position.x,position.y,position.z);
        double my_scale_val = (1.0 + sqrt(scale_val)*0.05);
        if (distance <= 0.5){
            glPopMatrix();
            return;
        }
        else {
            my_scale_val = my_scale_val*0.1*(distance-0.5);
            std::cerr << distance << std::endl;
        }
        glScalef(my_scale_val,my_scale_val,my_scale_val);
        glRotatef(rotate_val,rot_axis.x,rot_axis.y,rot_axis.z);
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
        rot_axis = position - sf::Vector3f(0.0,cam_pos,0.0);
        distance = sqrt(rot_axis.x*rot_axis.x+rot_axis.y*rot_axis.y+rot_axis.z*rot_axis.z);
        scale_val = scale_val * 0.7 + (float)freq_vals[frame][2]*0.3;
        rotate_speed = rotate_speed * 0.2 + (float)freq_vals[frame][0]*0.5*0.8;
        rotate_val += 3.0 - rotate_speed * 0.25;
    }
};

class Cube_Tube{
private:
    Cube cubes[8][26];
public:
    void init(){
        for (int i = 0; i < 8; i++){
            for (int j = 0; j < 26; j++){
                if (j < 5){
                    cubes[i][j].set_position(sf::Vector3f(5.0,0.0,0.0));
                }
                else if (j < 13){
                    cubes[i][j].set_position(sf::Vector3f(8.0,0.0,0.0));
                }
                else if (j < 26){
                    cubes[i][j].set_position(sf::Vector3f(13.0,0.0,0.0));
                }
            }
        }
    }
    void draw(){
        for (int i = 0; i < 8; i++){
            glPushMatrix();
            glTranslatef(0.0,i*2.0,0.0);
            for (int j = 0; j < 26; j++){
                glPushMatrix();
                if (j < 5){
                    glRotatef(j*72.0,0.0,1.0,0.0);
                    if (i%2==0){
                        glScalef(-1.0,1.0,1.0);
                        glRotatef(-cam_rotation,0.0,1.0,0.0);
                    }
                }
                else if (j < 13){
                    glRotatef((j-5)*360.0/8.0,0.0,1.0,0.0);
                    if (i%2!=0){
                        glScalef(-1.0,1.0,1.0);
                        glRotatef(-cam_rotation,0.0,1.0,0.0);
                    }
                }
                else {
                    glRotatef((j-13)*360.0/13.0,0.0,1.0,0.0);
                    if (i%2==0){
                        glScalef(-1.0,1.0,1.0);
                        glRotatef(-cam_rotation,0.0,1.0,0.0);
                    }
                }
                cubes[i][j].draw();
                glPopMatrix();
            }
            glPopMatrix();
        }
    }
    void update(){
        for (int i = 0; i < 8; i++){
            for (int j = 0; j < 26; j++){
                cubes[i][j].update();
            }
        }
        
    }
};

Cube_Tube cube_tube;*/

GLfloat light_magnitude = 0.0;

void display(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    
    glLoadIdentity();
    gluPerspective(45.0,(float)screen_width/(float)screen_height,0.1,200);
    
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    
    
    //MODIFY THE CAMERA HERE
    glRotatef(cam_rotation,0.0,1.0,0.0);
    glTranslatef(-cam_pos.x,-cam_pos.y,-cam_pos.z);
    /*if (frame < 240){
        cam_pos.z += 24.0/240.0;
    }
    if (frame > 520){
        cam_pos.z -= 48.0/140.0;
    }*/
    //EDIT LIGHTING HERE
    light_magnitude = 1.0;
    GLfloat light_ambient[4];
    GLfloat light_diffuse[] = {light_magnitude,light_magnitude,light_magnitude,1.0};
    GLfloat light_specular[] = {light_magnitude,light_magnitude,light_magnitude,1.0};
    GLfloat light_position[] = {0.0, 0.0, 0.0, 1.0};
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glEnable(GL_LIGHT0);
    
    //DISPLAY STUFF HERE
    glTranslatef(0.0,0.0,-48.0);
    glTranslatef(0.0,-12.0,0.0);
    glRotatef(15.0,1.0,0.0,0.0);
    hex_floor.draw();
    //kaleido_floor.draw();
    //glRotatef(frame,1.0,1.0,1.0);
    //draw_cubes();
    
    //GLfloat color[] = {1.0,1.0,1.0};
	//draw_1x1_cube(true,color);
    //draw_cube_sphere(7.5);
}

void update(){
    frame++;
    hex_floor.update();
    //kaleido_floor.update();
    //cam_pos += cam_move_speed;
    //cam_rotation += cam_rotate_speed;
}

void init(){
    floor_texture.loadFromFile("floor.png");
	std::ifstream f("vals.txt");
    std::string in;
    int curr_val = 0;
    for (int i = 0; i < FRAMES; i++){
        f >> in;
        unsigned int ind = 0;
        for (int j = 0; j < 3; j++){
            while (true){
                if (ind == in.length() || in[ind] == ','){
                    freq_vals[i][j] = (double)curr_val;
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
        }
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
    hex_floor.init();
    //kaleido_floor.init();
    //assign_cubes();
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
        if (screenshot_count%10==0) printf("frame %d\n",screenshot_count);
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
    	
#endif
    	window->display();
    }

	return 1;
}