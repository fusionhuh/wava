#include <math.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <mutex>
#include <thread>
#include "graphics.h"

// Based HEAVILY on: https://www.a1k0n.net/2011/07/20/donut-math.html



Color::Color() {}
Color::Color(uint8_t r, uint8_t g, uint8_t b) {
	Color::r = r;
	Color::g = g;
	Color::b = b;
}

Color operator+(const Color color1, const Color color2) {
    return Color(color1.r + color2.r, color1.g + color2.g, color1.b + color2.b);
}

Color operator*(const double scalar, const Color color) {
    return Color(color.r * scalar, color.g * scalar, color.b * scalar);
}

bool operator==(const Color color1, const Color color2) {
    if (color1.r == color2.r && color1.g == color2.g && color1.b == color2.b) return true;
    else return false;
}

ColorTag::ColorTag() {}
ColorTag::ColorTag(Color color, float luminance) { this->color = color; this->luminance = luminance; }

Color Shape::calculate_corresponding_color(float normalized_val) {
    int palette_size = palette.colors.size();
    Color color;

    if (palette.symmetric) {
        int palette_index = normalized_val * (palette_size + ((int) ((palette_size/2) + 1)));
        if (palette_index == (palette_size + ((int) (palette_size/2) + 1))) palette_index--;
        if (palette_index >= palette_size) palette_index -= palette_size;

        color = palette.colors[palette_index];
    }
    else {
        int palette_index = normalized_val * (palette_size);
        if (palette_index == palette_size) palette_index--;

        color = palette.colors[palette_index];
    }

    return color;
}

int Shape::get_shape_type() { return 0; }
int TriPrism::get_shape_type() { return TRI_PRISM_SHAPE; }
int Sphere::get_shape_type() { return SPHERE_SHAPE; }
int Donut::get_shape_type() { return DONUT_SHAPE; }
int RectPrism::get_shape_type() { return RECT_PRISM_SHAPE; }
int Circle::get_shape_type() { return CIRCLE_SHAPE; }
int Disc::get_shape_type() { return DISC_SHAPE; }
int Triangle::get_shape_type() { return TRIANGLE_SHAPE; }

ColorPalette generate_rand_palette() {
    int palette_num = rand() % WAVA_PALETTE_COUNT;
    ColorPalette palette;

    switch (palette_num) {
        case TRANS_FLAG_PALETTE:
            palette.name = std::string("trans");
            palette.symmetric = false;
            palette.colors.push_back(Color(0xFF, 0xFF, 0xFF) /*white*/);
            palette.colors.push_back(Color(0xF5, 0xA9, 0xB8) /*pink*/);
            palette.colors.push_back(Color(0x5B, 0xCE, 0xFA) /*light blue*/); // there's probably a more space-efficient way to do this
        break;
        case PRIDE_FLAG_PALETTE:
            palette.name = std::string("pride");
            palette.symmetric = false;
            palette.colors.push_back(Color(0xD1, 0x22, 0x29) /*red*/);
            palette.colors.push_back(Color(0xF6, 0x8A, 0x1E) /*orange*/);
            palette.colors.push_back(Color(0xFD, 0xE0, 0x1A) /*yellow*/);
            palette.colors.push_back(Color(0x00, 0x79, 0x40) /*green*/);
            palette.colors.push_back(Color(0x24, 0x40, 0x8E)); // blue
            palette.colors.push_back(Color(0x73, 0x29, 0x82) /*violet*/);
        break;
        default:
            fputs("Invalid palette type generated.", stderr);
            exit(-1);
        break;
    }
    return palette;
}

void normalize_vector(std::vector<double>& vec) {
	double magnitude = 0.0000001;
	for (int i = 0; i < vec.size(); i++) magnitude+=vec[i]*vec[i];
	magnitude = sqrt(magnitude);
	for (int i = 0; i < vec.size(); i++) vec[i]/=magnitude;
}

double operator*(std::vector<double> vec1, std::vector<double> vec2) {
	if (vec1.size() != vec2.size()) { fputs("Invalid vector dimensions for dot product.\n", stderr); exit(-1); }
	double dot = 0;
	for (int i = 0; i < vec1.size(); i++) {
		dot+=vec1[i] * vec2[i];
	}
	return dot;
}

std::vector<Shape*> generate_rand_shapes(const int count, const float variance, const int freq_bands) {
    std::vector<Shape*> shapes;
    for (int i = 0; i < count; ++i) {
        int shape_type = rand() % 3;
        switch(DONUT_SHAPE) {
            case RECT_PRISM_SHAPE:
            {
                RectPrism* rect_prism = new RectPrism();
                rect_prism->height = 1.5;
                rect_prism->width = 1.5;
                rect_prism->depth = 1.5;

                rect_prism->base_luminance = 1.1;

                rect_prism->palette = generate_rand_palette();

                shapes.push_back(rect_prism);
            }
            break;
            case SPHERE_SHAPE:
            {
                Sphere* sphere = new Sphere();
                sphere->radius = 1.0;

                sphere->base_luminance = 1.2;

                sphere->palette = generate_rand_palette();

                shapes.push_back(sphere);
            }
            break;
            case DONUT_SHAPE:
            {
                Donut* donut = new Donut();
                donut->radius = 0.5;
                donut->thickness = 0.25;

                donut->base_luminance = 2;

                donut->palette = generate_rand_palette();

                donut->radius_weighting_function = std::vector<double>(freq_bands);
                donut->radius_weighting_function[0] = 1;
                normalize_vector(donut->radius_weighting_function);
                donut->thickness_weighting_function = std::vector<double>(freq_bands);
                //donut->thickness_weighting_function[9] = 2;
                normalize_vector(donut->thickness_weighting_function);
                donut->luminance_weighting_function = std::vector<double>(freq_bands);
                //donut->luminance_weighting_function[0] = 2;
                normalize_vector(donut->luminance_weighting_function);

                shapes.push_back(donut);
            }
            break;
            default:
                fputs("Invalid shape type generated.", stderr);
                exit(-1);
            break;
        }
    }

    return shapes;
}

vec3 vec3::operator+(const vec3& vec) { return { this->x + vec.x, this->y + vec.y, this->z + vec.z }; }
vec3 vec3::operator-(const vec3& vec) { return { this->x - vec.x, this->y - vec.y, this->z - vec.z }; } // vector sub
vec3 vec3::operator*(const float& scalar) { return { this->x * scalar, this->y * scalar, this->z * scalar  }; } // scalar mult
vec3 vec3::operator/(const float& scalar) { return { this->x / scalar, this->y / scalar, this->z / scalar }; } // scalar div
float vec3::operator*(const vec3& vec) { return (this->x * vec.x + this->y * vec.y + this->z * vec.z); } // dot
float vec3::magnitude() { return sqrt (this->x * this->x + this->y * this->y + this->z * this->z); }; 

void vec3::normalize() { 
    float magnitude_inverse = 1/this->magnitude();
    this->x = this->x*magnitude_inverse;
    this->y = this->y*magnitude_inverse;
    this->z = this->z*magnitude_inverse;
}

vec3 vec3::operator^(const vec3& vec) { // cross
    float a = (this->y * vec.z - this->z * vec.y);
    float b = -1 * (this->x * vec.z - this->z * vec.x);
    float c = (this->x * vec.y - this->y * vec.x);
    return { a, b, c }; 
}

matrix3::matrix3(const char type, float theta = 0, float phi = 0) {
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
      exit(-1);
    break;
  }
}

vec3 wava_screen::light = (vec3) {1, 0, -1};
float wava_screen::light_smoothness = -1;

wava_screen::wava_screen(int x, int y) {
	zbuffer = std::vector<double>(x * y);
	output = std::vector<ColorTag>(x * y);

    light.normalize();

    background_print_str = std::string("@@");
    shape_print_str = std::string("XX");

    this->x = x;
    this->y = y;

    R1 = 0.5;
    R2 = 2;

    K2 = 15;
    // Calculate K1 based on screen size: the maximum x-distance occurs
    // roughly at the edge of the torus, which is at x=R1+R2, z=0.  we
    // want that to be displaced 3/8ths of the width of the screen, which
    // is 3/4th of the way from the center to the side of the screen.
    //SCREEN_WIDTH*3/8 = K1*(R1+R2)/(K2+0)
    //SCREEN_WIDTH*K2*3/(8*(R1+R2)) = K1
    K1 = 50*K2*3/(8*(R1+R2)); // need to update to use wava_screen values

    bg_palette = generate_rand_palette();
}

int wava_screen::x_dim() { return x; }
int wava_screen::y_dim() { return y; }

int wava_screen::get_index(int curr_x, int curr_y) {
  return (curr_x * y + curr_y);
}

const char* wava_screen::get_shape_print_str() {
    return shape_print_str.c_str();
}

const char* wava_screen::get_background_print_str() {
    return background_print_str.c_str();
}

std::tuple<int, int, float> wava_screen::calculate_proj_coord(vec3 pos) {
    pos.z += K2;
    float ooz = 1/pos.z;

    int xp = (x_dim() * 0.5 + K1*ooz*pos.x);
    int yp = (y_dim() * 0.5 - K1*ooz*pos.y);
    if (xp >= x_dim()) xp = x_dim() - 1; // checking to see if projected coord is out of screen bounds
    else if (xp < 0) xp = 0;
    if (yp < 0) yp = 0;
    else if  (yp >= y_dim()) yp = y - 1;

    return std::tuple<int, int, float>(xp, yp, ooz);
}

void wava_screen::write_to_z_buffer_and_output(const float* zbuffer, const ColorTag* output) {
  mtx.lock();
  for(int x = 0; x < x_dim(); x++) {
    for(int y = 0; y < y_dim(); y++) {
	  int curr_index = get_index(x,y);
      if (zbuffer[curr_index] > this->zbuffer[curr_index]) {
        this->zbuffer[curr_index] = zbuffer[curr_index];
        this->output[curr_index] = output[curr_index];
      }
    }
  }
  mtx.unlock();
}

vec3 operator*(vec3 vec, matrix3 mat) { return { vec * mat.col_one, vec * mat.col_two, vec * mat.col_three }; }

void draw_donut (Donut donut, wava_screen &screen, const std::vector<double> wava_out, const float A, const float B) {
    float radius = donut.radius, thickness = donut.thickness, luminance = donut.base_luminance;
    double radius_increase = donut.radius_weighting_function * wava_out * 0.5;
    double thickness_increase = donut.thickness_weighting_function * wava_out;
    double luminance_increase = donut.luminance_weighting_function * wava_out * 2;

    radius*=(radius_increase + 1)*(radius_increase+1);
    thickness*=(thickness_increase + 1);
    luminance*=(luminance_increase + 1);
    
    float* ooz_data = new float[screen.x_dim() * screen.y_dim()]();
    ColorTag* output_data = new ColorTag[screen.x_dim() * screen.y_dim()]();

    matrix3 matrix_x = matrix3 ('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float theta=0; theta < 2*PI; theta += THETA_SPACING) {
        for (float phi=0; phi < 2*PI; phi += PHI_SPACING) {
            vec3 pos = { radius + thickness * cos(theta) + 1, thickness * sin(theta), 0.0001 }; // setting z to small num to avoid NaN with 1/z

            matrix3 matrix_y = matrix3 ('y', phi);

            vec3 transformed_pos = pos * matrix_y * matrix_x * matrix_z;
            
            std::tuple<int, int, float> coord = screen.calculate_proj_coord(transformed_pos);
            int xp = std::get<0>(coord), yp = std::get<1>(coord);
            float ooz = std::get<2>(coord);

            vec3 normal = (vec3) {cos (theta), sin (theta), 0} * matrix_y * matrix_x * matrix_z;
            normal.normalize();

            float L = (normal * screen.light);

            L += (log(luminance + 1)*log2_inverse) - 1;
            if (L > 1) L = 1;
            curr_tag.luminance = L;

            if (L < 0) { // only important to check for color if luminance is larger than 0
                L = 0; 
                curr_tag.color = Color(0, 0, 0);
            }
            else {
                float dist_from_center = (thickness * cos(theta) + thickness)/(2 * thickness);
                curr_tag.color = donut.calculate_corresponding_color(dist_from_center);
            }

            int arr_index = screen.get_index(xp, yp);

            if (ooz > ooz_data[arr_index]) { ooz_data[arr_index] = ooz; output_data[arr_index] = curr_tag; }
        }
    }
    screen.write_to_z_buffer_and_output(ooz_data, output_data);
    delete [] ooz_data; delete [] output_data;
}

void draw_sphere (Sphere sphere, wava_screen &screen, const std::vector<double> weighting_coefficients, const float A, const float B) {
    float radius = sphere.radius;

    float* ooz_data = new float[screen.x_dim()* screen.y_dim()]();
    ColorTag* output_data = new ColorTag[screen.x_dim()* screen.y_dim()]();
  
    matrix3 matrix_x = matrix3 ('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float theta = 0; theta < 2*PI; theta += THETA_SPACING) {
        for (float phi = 0; phi < 1*PI; phi += PHI_SPACING) {
            vec3 pos = {radius * cos (theta), radius * sin (theta), 0.00000001};

            matrix3 matrix_y = matrix3 ('y', phi);

            vec3 transformed_pos = pos * matrix_y * matrix_x * matrix_z;

            std::tuple<int, int, float> coord = screen.calculate_proj_coord(transformed_pos);
            int xp = std::get<0>(coord), yp = std::get<1>(coord);
            float ooz = std::get<2>(coord);

            vec3 normal = transformed_pos;
            normal.normalize();

            float L = (normal * screen.light);

            L += (log(sphere.base_luminance + 1)*log2_inverse) - 1;
            if (L > 1) L = 1;
            curr_tag.luminance = L;

            if (L < 0) { // only important to check for color if luminance is larger than 0
                L = 0; 
                curr_tag.color = Color(0, 0, 0);
            }
            else {
                float dist_from_center = (radius * cos(theta)) + radius/2;
                sphere.calculate_corresponding_color(dist_from_center);
            }
            curr_tag.color = Color(255, 255, 255);

            int arr_index = screen.get_index(xp, yp);

            if (ooz > ooz_data[arr_index]) { ooz_data[arr_index] = ooz; output_data[arr_index] = curr_tag; }
        }
    }
    screen.write_to_z_buffer_and_output(ooz_data, output_data);
    delete [] ooz_data; delete [] output_data;
}

void draw_rect_prism (RectPrism rect_prism, wava_screen &screen, const std::vector<double> weighting_coefficients, const float A, const float B) {
    float width = rect_prism.width;
    float height = rect_prism.height;
    float depth = rect_prism.depth;

    float* ooz_data = new float[screen.x_dim()* screen.y_dim()]();
    ColorTag* output_data = new ColorTag[screen.x_dim()* screen.y_dim()]();

    matrix3 matrix_x = matrix3('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float x = 0; x < width; x += PRISM_SPACING) {
        for (float y = 0; y < height; y += PRISM_SPACING) {
            vec3 side1_front = { x, y, 0 };
            vec3 side1_back = { x, y, depth };

            vec3 side2_left = (vec3) {0, y, (x/width) * depth};
            vec3 side2_right = side2_left + (vec3) {width, 0, 0};

            vec3 side3_bottom = (vec3) {x, 0, (y/height)*depth};
            vec3 side3_top = side3_bottom + (vec3) {0, height, 0};

            vec3 curr_points[6] = { side1_front, side1_back, side2_left, side2_right, side3_bottom, side3_top };
            for (int i = 0; i < 6; i++) {
                vec3 transformed_pos = curr_points[i] - (vec3) {width/2, height/2, depth/2};
                transformed_pos = transformed_pos * matrix_x * matrix_z;
                transformed_pos = transformed_pos + (vec3) {0, 0, 0.0000000001};

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

                normal = normal * matrix_x * matrix_z;
                normal.normalize();

                std::tuple<int, int, float> coord = screen.calculate_proj_coord(transformed_pos);
                int xp = std::get<0>(coord), yp = std::get<1>(coord);
                float ooz = std::get<2>(coord);             

                float L = (normal * screen.light);

                L += (log(rect_prism.base_luminance + 1)*log2_inverse) - 1;
                if (L > 1) L = 1;
                curr_tag.luminance = L;

                if (L < 0) { // only important to check for color if luminance is larger than 0
                    L = 0; 
                    curr_tag.color = Color(0, 0, 0);
                }
                else {
                    float dist_from_center = x/width;
                    rect_prism.calculate_corresponding_color(dist_from_center);
                }

                int arr_index = screen.get_index(xp, yp);

                if (ooz > ooz_data[arr_index]) { ooz_data[arr_index] = ooz; output_data[arr_index] = curr_tag; }
            }
        }
    }
  
  screen.write_to_z_buffer_and_output(ooz_data, output_data);
  delete [] ooz_data; delete [] output_data;
}


