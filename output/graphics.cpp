#include <math.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <mutex>
#include <thread>
#include <iostream>
#include <stdlib.h>

#include <graphics.hpp>

// Based HEAVILY on: https://www.a1k0n.net/2011/07/20/donut-math.html

Color::Color() {}
Color::Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {} 

Color Color::operator+(const Color& color2) {
    return Color(
        (this->r + color2.r <= 255) ? this->r + color2.r : 255,
        (this->g + color2.g <= 255) ? this->g + color2.g : 255,
        (this->b + color2.b <= 255) ? this->b + color2.b : 255
    );
}

Color operator*(double scalar, const Color& color) {
    return Color(color.r * scalar, color.g * scalar, color.b * scalar);
}

bool operator==(const Color& color1, const Color& color2) {
    if (color1.r == color2.r && color1.g == color2.g && color1.b == color2.b) return true;
    else return false;
}

ColorTag::ColorTag() {}
ColorTag::ColorTag(const Color& color, float lum) : luminance(lum) { this->color = color; }

Color Shape::calculate_corresponding_color(float normalized_val) {
    int palette_size = palette.colors.size();

    if (highlight) {
        unsigned char val = rand() % 256;
        return Color(val, val, val);
    }

    if (palette.symmetric) {
        int palette_index = normalized_val * (palette_size + ((int) ((palette_size/2) + 1)));
        if (palette_index == (palette_size + ((int) (palette_size/2) + 1))) palette_index--;
        if (palette_index >= palette_size) palette_index -= palette_size;

        return palette.colors[palette_index];
    }
    else {
        int palette_index = normalized_val * (palette_size);
        if (palette_index == palette_size) palette_index--;

        return palette.colors[palette_index];
    }
}

int Shape::get_shape_type() { return 0; }
int TriPrism::get_shape_type() { return TRI_PRISM_SHAPE; }
int Sphere::get_shape_type() { return SPHERE_SHAPE; }
int Donut::get_shape_type() { return DONUT_SHAPE; }
int RectPrism::get_shape_type() { return RECT_PRISM_SHAPE; }
int Triangle::get_shape_type() { return TRIANGLE_SHAPE; }

void Shape::increase_size() {}
void Donut::increase_size() { radius+=0.05; thickness+=0.02; }
void Sphere::increase_size() { radius+=0.05; }
void RectPrism::increase_size() { width+=0.05; depth+=0.05; height+=0.05; }

void Shape::decrease_size() {}
void Donut::decrease_size() {
    if (radius >= 0.1) radius-=0.05;
    if (thickness >= 0.04) thickness-=0.02;
}
void Sphere::decrease_size() {
    if (radius >= 0.1) radius-=0.05;
}
void RectPrism::decrease_size() {
    if (width >= 0.1) { width-=0.05; depth-=0.05; height-=0.05; }
}

ColorPalette generate_palette(int index) {
    if (index == -1) index = rand() % WAVA_PALETTE_COUNT;
    ColorPalette palette;

    switch (index) {
        case TRANS_FLAG_PALETTE:
        {
            std::vector<Color> colors = { Color(0xFF, 0xFF, 0xFF), Color(0xFF, 0xFF, 0xFF), Color(0xFF, 0xFF, 0xFF) };
            palette = ColorPalette { std::string("trans"), false, colors };
        }
        break;
        case PRIDE_FLAG_PALETTE:
        {   
            std::vector<Color> colors = { Color(0xD1, 0x22, 0x29), Color(0xF6, 0x8A, 0x1E), Color(0xFD, 0xE0, 0x1A), 
                                    Color(0x00, 0x79, 0x40), Color(0x24, 0x40, 0x8E), Color(0x73, 0x29, 0x82) };
            palette = ColorPalette { std::string("pride"), false, colors };
        }
        break;
        case EERIE_PALETTE:
        {   
            std::vector<Color> colors = { Color(0x1A, 0x18, 0x1B), Color(0x56, 0x4D, 0x65), Color(0x3E, 0x89, 0x89), 
                                    Color(0x2C, 0xDA, 0x9D), Color(0x05, 0xF1, 0x40) };
            palette = ColorPalette { std::string("eerie"), false, colors };
        }
        break;
        default:
        {
            std::cerr << "Invalid palette color specified, defaulting to pride.";
            return generate_palette(0);
        }
        break;
    }
    return palette;
}

Shape::Shape(float x_offset, float y_offset, float base_luminance, int freq_bands, int color_index, int shape_type) :
    x_offset(x_offset), y_offset(y_offset), base_luminance(base_luminance), velocity(0), highlight(false), shape_type(shape_type)
{
    luminance_weighting_function = std::vector<double>(freq_bands);
    palette = generate_palette(color_index);
}

Donut::Donut(float radius, float thickness, float x_offset, float y_offset, float base_luminance, int freq_bands, int color_index) :
    radius(radius), thickness(thickness), Shape(x_offset, y_offset, base_luminance, freq_bands, color_index, DONUT_SHAPE)
{
    
}

Sphere::Sphere(float radius, float x_offset, float y_offset, float base_luminance, int freq_bands, int color_index) :
    radius(radius), Shape(x_offset, y_offset, base_luminance, freq_bands, color_index, SPHERE_SHAPE)
{

}

RectPrism::RectPrism(float height, float width, float depth, float x_offset, float y_offset, float base_luminance, int freq_bands, int color_index) :
    height(height), width(width), depth(depth), Shape(x_offset, y_offset, base_luminance, freq_bands, color_index, RECT_PRISM_SHAPE)
{

}

void normalize_vector(std::vector<double>& vec) {
	double magnitude = 0.00001;
	for (int i = 0; i < vec.size(); i++) magnitude+=vec[i]*vec[i];
	magnitude = sqrt(magnitude);
	for (int i = 0; i < vec.size(); i++) vec[i]/=magnitude;
}

double operator*(const std::vector<double>& vec1, const std::vector<double>& vec2) {
	if (vec1.size() != vec2.size()) { std::cerr << "Invalid dimensions for dot product." ; exit(-1); }
	double dot = 0;
	for (int i = 0; i < vec1.size(); i++) {
		dot+=vec1[i] * vec2[i];
	}
	return dot;
}

std::vector<Shape*> generate_shapes(int donut_count, int sphere_count, int rect_prism_count, float variance, int freq_bands, int color_index) {
    std::vector<Shape*> shapes;

    for (int i = 0; i < donut_count; i++) {
        Donut* donut = new Donut(0.5, 0.2, 0, 0, 2, freq_bands, color_index);

        donut->radius_weighting_function = std::vector<double>(freq_bands);
        donut->radius_weighting_function[0] = 1;
        donut->thickness_weighting_function = std::vector<double>(freq_bands);
        donut->thickness_weighting_function[0] = 1;
        donut->luminance_weighting_function = std::vector<double>(freq_bands);
        donut->luminance_weighting_function[0] = 1;

        shapes.push_back(donut);
    }
    for (int i = 0; i < sphere_count; i++) {
        Sphere* sphere = new Sphere(0.5, 0, 0, 2, freq_bands, color_index);

        sphere->highlight = false;

        sphere->radius_weighting_function = std::vector<double>(freq_bands);
        sphere->radius_weighting_function[0] = 0.5;
        sphere->luminance_weighting_function = std::vector<double>(freq_bands);
        sphere->luminance_weighting_function[0] = 1;

        shapes.push_back(sphere);
    }
    for (int i = 0; i < rect_prism_count; i++) {
        RectPrism* rect_prism = new RectPrism(1, 1, 1, 0, 0, 2, freq_bands, color_index);

        rect_prism->volume_weighting_function = std::vector<double>(freq_bands);
        rect_prism->volume_weighting_function[0] = 1;
        rect_prism->luminance_weighting_function = std::vector<double>(freq_bands);
        rect_prism->luminance_weighting_function[0] = 1;

        shapes.push_back(rect_prism);
    }

    return shapes;
}

vec3 vec3::operator+(const vec3& vec) { return { this->x + vec.x, this->y + vec.y, this->z + vec.z }; }
vec3 vec3::operator-(const vec3& vec) { return { this->x - vec.x, this->y - vec.y, this->z - vec.z }; } // vector sub
vec3 vec3::operator*(float scalar) { return { this->x * scalar, this->y * scalar, this->z * scalar  }; } // scalar mult
vec3 vec3::operator/(float scalar) { return { this->x / scalar, this->y / scalar, this->z / scalar }; } // scalar div
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

wava_screen::wava_screen(int x, int y, float theta, float phi, float prism, float smoothness, int palette_index) : 
    x(x), y(y), theta_spacing(theta), phi_spacing(phi), prism_spacing(prism), light_smoothness(smoothness), bg_palette_index(palette_index), 
    zbuffer(x * y), output(x * y), background_print_str("██"), shape_print_str("██")
{
    light.normalize();

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

    bg_palette = generate_palette(bg_palette_index);
}

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

    int xp = (this->x * 0.5 + K1*ooz*pos.x);
    int yp = (this->y * 0.5 - K1*ooz*pos.y);
    if (xp >= this->x) xp = this->x - 1; // checking to see if projected coord is out of screen bounds
    else if (xp < 0) xp = 0;
    if (yp < 0) yp = 0;
    else if  (yp >= this->y) yp = this->y - 1;

    return std::tuple<int, int, float>(xp, yp, ooz);
}

void wava_screen::write_to_z_buffer_and_output(const float* zbuffer, const ColorTag* output) {
  mtx.lock();
  for(int x = 0; x < this->x; x++) {
    for(int y = 0; y < this->y; y++) {
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

void draw_donut (Donut donut, wava_screen &screen, std::vector<double> wava_out, float A, float B) {
    double theta_spacing = (donut.highlight) ? THETA_SPACING : screen.theta_spacing;
    double phi_spacing = (donut.highlight) ? PHI_SPACING : screen.phi_spacing;

    double radius_increase = (donut.radius_weighting_function * wava_out * 0.5) + 1;
    double thickness_increase = (donut.thickness_weighting_function * wava_out) + 1;
    double luminance_increase = (donut.luminance_weighting_function * wava_out * 2) + 1;

    float radius = donut.radius * radius_increase * radius_increase;
    float thickness = donut.thickness * thickness_increase;
    float luminance = donut.base_luminance * luminance_increase;
    
    
    float* ooz_data = new float[screen.x * screen.y]();
    ColorTag* output_data = new ColorTag[screen.x * screen.y]();

    matrix3 matrix_x = matrix3 ('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float theta=0; theta < 2*PI; theta += theta_spacing) {
        for (float phi=0; phi < 2*PI; phi += phi_spacing) {
            vec3 pos = { radius + thickness * cos(theta) + 1, thickness * sin(theta), 0.0001 }; // setting z to small num to avoid NaN with 1/z

            matrix3 matrix_y = matrix3 ('y', phi);

            vec3 transformed_pos = pos * matrix_y * matrix_x * matrix_z;

            transformed_pos = transformed_pos + vec3 { donut.x_offset, donut.y_offset, 0 };
            
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

void draw_sphere (Sphere sphere, wava_screen &screen, std::vector<double> wava_out, float A, float B) {
    double theta_spacing = (sphere.highlight) ? THETA_SPACING : screen.theta_spacing;
    double phi_spacing = (sphere.highlight) ? PHI_SPACING : screen.phi_spacing;

    double radius_increase = (sphere.radius_weighting_function * wava_out) + 1;
    double luminance_increase = (sphere.luminance_weighting_function * wava_out) + 1;
    
    float radius = sphere.radius * radius_increase;
    float luminance = sphere.base_luminance * luminance_increase;

    float* ooz_data = new float[screen.x * screen.y]();
    ColorTag* output_data = new ColorTag[screen.x * screen.y]();
  
    matrix3 matrix_x = matrix3 ('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float theta = 0; theta < 2*PI; theta += theta_spacing) {
        for (float phi = 0; phi < 1*PI; phi += phi_spacing) {
            vec3 pos = {radius * cos (theta), radius * sin (theta), 0.00000001};

            matrix3 matrix_y = matrix3 ('y', phi);

            vec3 transformed_pos = pos * matrix_y * matrix_x * matrix_z;

            vec3 normal = transformed_pos;
            normal.normalize();

            transformed_pos = transformed_pos + vec3 { sphere.x_offset, sphere.y_offset, 0 };

            std::tuple<int, int, float> coord = screen.calculate_proj_coord(transformed_pos);
            int xp = std::get<0>(coord), yp = std::get<1>(coord);
            float ooz = std::get<2>(coord);

            float L = (normal * screen.light);

            L += (log(luminance + 1)*log2_inverse) - 1;
            if (L > 1) L = 1;
            curr_tag.luminance = L;

            if (L < 0) { // only important to check for color if luminance is larger than 0
                L = 0; 
                curr_tag.color = Color(0, 0, 0);
            }
            else {
                float dist_from_center = abs(radius*sin(theta))/radius;
                //float dist_from_center = (radius * cos(theta)) + radius/2;
                curr_tag.color = sphere.calculate_corresponding_color(dist_from_center);
            }

            int arr_index = screen.get_index(xp, yp);

            if (ooz > ooz_data[arr_index]) { ooz_data[arr_index] = ooz; output_data[arr_index] = curr_tag; }
        }
    }
    screen.write_to_z_buffer_and_output(ooz_data, output_data);
    delete [] ooz_data; delete [] output_data;
}

void draw_rect_prism (RectPrism rect_prism, wava_screen& screen, std::vector<double> wava_out, float A, float B) {
    double prism_spacing = (rect_prism.highlight) ? PRISM_SPACING : screen.prism_spacing;

    double volume_increase = (rect_prism.volume_weighting_function * wava_out * 0.5) + 1;
    double luminance_increase = (rect_prism.luminance_weighting_function * wava_out) + 1;

    float width = rect_prism.width * volume_increase;
    float height = rect_prism.height * volume_increase;
    float depth = rect_prism.depth * volume_increase;

    float luminance = rect_prism.base_luminance * luminance_increase;

    float* ooz_data = new float[screen.x * screen.y]();
    ColorTag* output_data = new ColorTag[screen.x * screen.y]();

    matrix3 matrix_x = matrix3('x', A), matrix_z = matrix3 ('z', B);

    ColorTag curr_tag;

    float log2_inverse = 1/log(2.0);
    for (float x = 0; x < width; x += prism_spacing) {
        for (float y = 0; y < height; y += prism_spacing) {
            vec3 side1_front { x, y, 0 };
            vec3 side1_back { x, y, depth };

            vec3 side2_left {0, y, (x/width) * depth};
            vec3 side2_right = side2_left + (vec3) {width, 0, 0};

            vec3 side3_bottom {x, 0, (y/height)*depth};
            vec3 side3_top = side3_bottom + (vec3) {0, height, 0};

            vec3 curr_points[6] = { side1_front, side1_back, side2_left, side2_right, side3_bottom, side3_top };
            for (int i = 0; i < 6; i++) {
                vec3 transformed_pos = curr_points[i] - (vec3) {width/2, height/2, depth/2};
                transformed_pos = transformed_pos * matrix_x * matrix_z;
                transformed_pos = transformed_pos + (vec3) {0, 0, 0.0000000001};

                transformed_pos = transformed_pos + vec3 { rect_prism.x_offset, rect_prism.y_offset, 0 };

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

                L += (log(luminance + 1)*log2_inverse) - 1;
                if (L > 1) L = 1;
                curr_tag.luminance = L;

                if (L < 0) { // only important to check for color if luminance is larger than 0
                    L = 0; 
                    curr_tag.color = Color(0, 0, 0);
                }
                else {
                    float dist_from_center = x/width;
                    curr_tag.color = rect_prism.calculate_corresponding_color(dist_from_center);
                }

                int arr_index = screen.get_index(xp, yp);

                if (ooz > ooz_data[arr_index]) { ooz_data[arr_index] = ooz; output_data[arr_index] = curr_tag; }
            }
        }
    }
  
  screen.write_to_z_buffer_and_output(ooz_data, output_data);
  delete [] ooz_data; delete [] output_data;
}


