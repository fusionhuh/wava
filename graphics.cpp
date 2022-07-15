#include "graphics.h"
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <string>


// Based HEAVILY on: https://www.a1k0n.net/2011/07/20/donut-math.html

const float theta_spacing = 0.07;
const float phi_spacing   = 0.02;
const float prism_spacing = 0.05;

const float pi = 3.14159265359;

const float R1 = 1;
const float R2 = 2;
const float K2 = 15;
// Calculate K1 based on screen size: the maximum x-distance occurs
// roughly at the edge of the torus, which is at x=R1+R2, z=0.  we
// want that to be displaced 3/8ths of the width of the screen, which
// is 3/4th of the way from the center to the side of the screen.
//SCREEN_WIDTH*3/8 = K1*(R1+R2)/(K2+0)
//SCREEN_WIDTH*K2*3/(8*(R1+R2)) = K1

const float K1 = SCREEN_WIDTH*K2*3/(8*(R1+R2));

const char* print_char = "  ";

struct vec3
{
    float x;
    float y;
    float z;

    vec3 operator+(const vec3& vec) { return { this->x + vec.x, this->y + vec.y, this->z + vec.z  }; } // vector add
    vec3 operator-(const vec3& vec) { return { this->x - vec.x, this->y - vec.y, this->z - vec.z }; } // vector sub

    vec3 operator*(const float& scalar) { return { this->x * scalar, this->y * scalar, this->z * scalar  }; } // scalar mult
    vec3 operator/(const float& scalar) { return { this->x / scalar, this->y / scalar, this->z / scalar }; } // scalar div

    float operator*(const vec3& vec) { return (this->x * vec.x + this->y * vec.y + this->z * vec.z); } // dot

    vec3 operator^(const vec3& vec) // cross
    {
        float a = (this->y * vec.z - this->z * vec.y);
        float b = -1 * (this->x * vec.z - this->z * vec.x);
        float c = (this->x * vec.y - this->y * vec.x);
        return { a, b, c }; 
    }

    float magnitude () { return sqrt (this->x * this->x + this->y * this->y + this->z * this->z); }; 

    void normalize () 
    { 
        float magnitude = this->magnitude();
        this->x = this->x/magnitude;
        this->y = this->y/magnitude;
        this->z = this->z/magnitude;
    }

    void print_coord () // debugging
    {
      printf ("X is: %f, Y is: %f, Z is: %f\n", x, y, z);
    }
};

struct color_tag 
{
  int type = 0;
  float luminance = 0;
};

struct color_palette {
  
};

struct matrix3
{
  vec3 col_one;
  vec3 col_two;
  vec3 col_three;
  
  matrix3(vec3 vec_one, vec3 vec_two, vec3 vec_three) {
    this->col_one = vec_one;
    this->col_two = vec_two;
    this->col_three = vec_three;
  }

  matrix3(const char type, float theta = 0, float phi = 0) {
    switch (type) 
    {
      case 'x':
        this->col_one = (vec3) {1, 0, 0};
        this->col_two = (vec3) {0, cos (theta), sin (theta)};
        this->col_three = (vec3) {0, -sin (theta), cos (theta)};
      break;
      case 'y':
        this->col_one = (vec3) {cos(theta), 0, -sin(theta)};
        this->col_two = (vec3) {0, 1, 0};
        this->col_three = (vec3) {sin (theta), 0, cos (theta)};
      break;
      case 'z':
        this->col_one = (vec3) {cos (theta), sin (theta), 0};
        this->col_two = (vec3) {-sin (theta), cos (theta), 0};
        this->col_three = (vec3) {0, 0, 1};
      break;
      default:
        fputs("Invalid matrix type specified.", stderr);
      break;
    }
  }
};

vec3 operator*(vec3 vec, matrix3 mat) 
{
  return { vec * mat.col_one, vec * mat.col_two, vec * mat.col_three };
}

void draw_donut (float A, float B, color_tag (&output)[SCREEN_HEIGHT][SCREEN_WIDTH], float (&zbuffer)[SCREEN_HEIGHT][SCREEN_WIDTH])
{
  for (float theta=0; theta < 2*pi; theta += theta_spacing) {
    for (float phi=0; phi < 2*pi; phi += phi_spacing) {
      float circlex = R2 + R1*cos (theta);
      float circley = R1*sin (theta);

      matrix3 matrix_one = matrix3 ('y', phi);
      matrix3 matrix_two = matrix3 ('x', A);
      matrix3 matrix_three = matrix3 ('z', B);

      color_tag curr_tag;
      if (circlex - R2 > 0) {
        int type = (int) (abs (circley/R1) * 3) + 1;
        if (type > 3) { type = 3; }
        curr_tag.type = type;
      } else {
        float diff = R1 - abs (circley);
        curr_tag.type = ((int) (6 * diff)) + 4;
      }

      vec3 pos = { circlex, circley, 0 };

      vec3 transformed_pos = pos * matrix_one * matrix_two * matrix_three;
      transformed_pos.z += K2;
      float ooz = 1/transformed_pos.z;
    
      int xp = (int) (SCREEN_WIDTH/2 + K1*ooz*transformed_pos.x);
      int yp = (int) (SCREEN_HEIGHT/2 - K1*ooz*transformed_pos.y);

      if (xp >= SCREEN_WIDTH) xp = SCREEN_WIDTH - 1;
      else if (xp < 0) xp = 0;
      if (yp < 0) yp = 0;
      else if  (yp >= SCREEN_HEIGHT) yp = SCREEN_HEIGHT - 1;

      vec3 normal = (vec3) {cos (theta), sin (theta), 0} * matrix_one * matrix_two * matrix_three;
      normal.normalize();

      vec3 light = {0, 1, -0.2};
      light.normalize();

      float L = normal * light;
      if (L < 0) { L = 0; }

      if(ooz > zbuffer[xp][yp]) {
        zbuffer[xp][yp] = ooz;
        float luminance_index = L;
        curr_tag.luminance = luminance_index;
        output[xp][yp] = curr_tag;
      }
    }
  }
}

void draw_sphere (float A, float B, color_tag (&output)[SCREEN_HEIGHT][SCREEN_WIDTH], float (&zbuffer)[SCREEN_HEIGHT][SCREEN_WIDTH])
{
  for (float theta = 0; theta < 2*pi; theta += theta_spacing) {
    for (float phi = 0; phi < 1*pi; phi += phi_spacing) {
      vec3 pos = {R1 * cos (theta), R1 * sin (theta), 0};

      matrix3 matrix_one = matrix3 ('y', phi);
      matrix3 matrix_two = matrix3 ('x', A);
      matrix3 matrix_three = matrix3 ('z', B);

      color_tag curr_tag;

      if (pos.y > 0) {
        curr_tag.type = 1;
      }
      else {
        curr_tag.type = 2;
      }

      vec3 transformed_pos = pos * matrix_one * matrix_two * matrix_three;
      transformed_pos.z += K2 + 4;
      float ooz = 1/transformed_pos.z;

      int xp = (int) (SCREEN_WIDTH/2 + K1*ooz*transformed_pos.x);
      int yp = (int) (SCREEN_HEIGHT/2 - K1*ooz*transformed_pos.y);
      if (xp >= SCREEN_WIDTH) xp = SCREEN_WIDTH - 1;
      else if (xp < 0) xp = 0;
      if (yp < 0) yp = 0;
      else if  (yp >= SCREEN_HEIGHT) yp = SCREEN_HEIGHT - 1;

      vec3 normal = transformed_pos;
      normal.normalize();

      vec3 light = {0, 1, 0.2};
      light.normalize();

      float L = normal * light;
      if (L < 0) L = 0;

      if (ooz > zbuffer[xp][yp]) {
        zbuffer[xp][yp] = ooz;
        float luminance_index = L;
        curr_tag.luminance = luminance_index;
        output[xp][yp] = curr_tag;
      }
    }
  }
}

void draw_prism (float A, float B, color_tag (&output)[SCREEN_HEIGHT][SCREEN_WIDTH], float (&zbuffer)[SCREEN_HEIGHT][SCREEN_WIDTH]) {
  float width = 3;
  float height = 3;
  float depth = 3;

  for (float x = 0; x < width; x += prism_spacing) {
    for (float y = 0; y < height; y += prism_spacing) {
        vec3 side1_front = { x, y, 0 };
        vec3 side1_back = { x, y, depth };

        vec3 light = (vec3) {0, 1, -1};
        light.normalize();

        vec3 side2_left = (vec3) {0, y, (x/width) * depth};
        vec3 side2_right = side2_left + (vec3) {width, 0, 0};

        vec3 side3_bottom = (vec3) {x, 0, (y/height)*depth};
        vec3 side3_top = side3_bottom + (vec3) {0, height, 0};

        vec3 curr_points[6] = { side1_front, side1_back, side2_left, side2_right, side3_bottom, side3_top };

        matrix3 matrix_two = matrix3('x', A);
        matrix3 matrix_three = matrix3 ('z', B);

        for (int i = 0; i < 6; i++) {
          vec3 transformed_pos = curr_points[i];
          transformed_pos = transformed_pos * matrix_two * matrix_three;
          transformed_pos = transformed_pos + (vec3) {0, 0, K2};

          vec3 normal; 
          switch (i) 
          {
            case 0: 
              normal = {0, 0, -1};
            break;
            case 1:
              normal = {0, 0, 1};
            break;
            case 2:
              normal = {-1, 0, 0};
            break;
            case 3:
              normal = {1, 0, 0};
            break;
            case 4:
              normal = {0, -1, 0};
            break;
            case 5:
              normal = {0, 1, 0};
            break;
          }
          normal = normal * matrix_two * matrix_three;
          normal.normalize();

          float ooz = 1/transformed_pos.z;
          int xp = (int) (SCREEN_WIDTH/2 + K1*ooz*transformed_pos.x);
          int yp = (int) (SCREEN_HEIGHT/2 - K1*ooz*transformed_pos.y);
          if (xp >= SCREEN_WIDTH) xp = SCREEN_WIDTH - 1;
          else if (xp < 0) xp = 0;
          if (yp < 0) yp = 0;
          else if  (yp >= SCREEN_HEIGHT) yp = SCREEN_HEIGHT - 1;
          
          float L = normal * light;
          if (L < 0) L = 0;

          if (ooz > zbuffer[xp][yp]) {
            zbuffer[xp][yp] = ooz;
            float luminance_index = L;
            color_tag curr_tag = { i, 0 };
            curr_tag.luminance = luminance_index;
            output[xp][yp] = curr_tag;
          }
        }
    }
  }
}

void render_frame (float A, float B)
{
  color_tag output[SCREEN_WIDTH][SCREEN_HEIGHT] = { 0, 0 };
  float zbuffer[SCREEN_WIDTH][SCREEN_HEIGHT] = { 0 };

  draw_donut (A, B, output, zbuffer);
  //draw_donut (A + 5, B + 5, output, zbuffer);
  //draw_sphere (A, B, output, zbuffer);
  //draw_donut (A + 10, B + 10, output, zbuffer);
  //draw_donut (A + 15, B + 15, output, zbuffer);
  //draw_donut (A + 20, B + 20, output, zbuffer);

  draw_prism (A+5, B+5, output, zbuffer);
  //draw_prism (A+10, B+10, output, zbuffer);
  //draw_prism (A, B, output, zbuffer);

  printf("\x1b[H");
  for (int j = 0; j < SCREEN_HEIGHT; j++) {
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      color_tag curr_tag = output[i][j];
      float brightness = curr_tag.luminance;
      int type = curr_tag.type;

      //printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x74), (int) (brightness * 0xD7), (int) (brightness * 0xEC), print_char);
      if (type == 0 || type == 1) printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x74), (int) (brightness * 0xD7), (int) (brightness * 0xEC), print_char);
      else if (type == 2 || type == 3) printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x74), (int) (brightness * 0x00), (int) (brightness * 0xEC), print_char);
      else if (type == 4 || type == 5) printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xFF), (int) (brightness * 0xFF), (int) (brightness * 0xEC), print_char);
      else printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xFF), (int) (brightness * 0x00), (int) (brightness * 0x00), print_char);
      /*switch (type)
      {
        case 3:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x74), (int) (brightness * 0xD7), (int) (brightness * 0xEC), print_char);
        break;
        case 2:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xFF), (int) (brightness * 0xAF), (int) (brightness * 0xC7), print_char);
        break;
        case 1:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xFB), (int) (brightness * 0xF9), (int) (brightness * 0xF5), print_char);
        break;
        case 4:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xD1), (int) (brightness * 0x22), (int) (brightness * 0x29), print_char);
        break;
        case 5:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xF6), (int) (brightness * 0x8A), (int) (brightness * 0x1E), print_char);
        break;
        case 6:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0xFD), (int) (brightness * 0xE0), (int) (brightness * 0x1A), print_char);
        break;
        case 7:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x00), (int) (brightness * 0x79), (int) (brightness * 0x40), print_char);
        break;
        case 8:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x24), (int) (brightness * 0x40), (int) (brightness * 0x8E), print_char);
        break;
        case 9:
        printf ("\x1b[48;2;%d;%d;%dm%s", (int) (brightness * 0x73), (int) (brightness * 0x29), (int) (brightness * 0x82), print_char);
        break;
        default: // no color, can be used for backgrounds
        printf ("\x1b[48;2;%d;%d;%dm  ", (int) (0x00), (int) (brightness * 0x00), (int) (brightness * 0x00));
        break;
      }*/
    }
    printf ("\n");
  }
}