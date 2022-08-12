#pragma once
#include <vector>
#include <mutex>
#include <string>

// color/shapes portion
#define RECT_PRISM_SHAPE 0
#define SPHERE_SHAPE 1
#define DONUT_SHAPE 2
#define TRI_PRISM_SHAPE 3
#define CIRCLE_SHAPE 4
#define DISC_SHAPE 5
#define TRIANGLE_SHAPE 6
#define WAVA_SHAPE_COUNT 7

#define RAND_PALETTE
#define PRIDE_FLAG_PALETTE 0
#define TRANS_FLAG_PALETTE 1
#define EERIE_PALETTE 2
#define WAVA_PALETTE_COUNT 3

#define BASS_WEIGHT_FUNCTION 0
#define MID_WEIGHT_FUNCTION 1
#define TREBLE_WEIGHT_FUNCTION 2
#define WAVA_WEIGHT_FUNCTION_COUNT 3

// geometry/matrix portion
#ifndef PI
#define PI 3.14159265359
#endif

struct vec3
{
    float x, y, z;

    vec3 operator+(const vec3& vec);
    vec3 operator-(const vec3& vec);

    vec3 operator*(float scalar);
    vec3 operator/(float scalar);

    float operator*(const vec3& vec);

    vec3 operator^(const vec3& vec);

    float magnitude (); 

    void normalize();

    void print_coord (); // debugging
};

struct matrix3
{
  vec3 col_one;
  vec3 col_two;
  vec3 col_three;
  
  matrix3(vec3 vec_one, vec3 vec_two, vec3 vec_three);

  matrix3(const char type, float theta, float phi);
};

vec3 operator*(vec3 vec, matrix3 mat);

void normalize_vector(std::vector<double>& vec);

double operator*(const std::vector<double>& vec1, const std::vector<double>& vec2);

// shapes/colors portion
struct Color {
	uint8_t r; uint8_t g; uint8_t b;

	Color();
	Color(uint8_t r, uint8_t g, uint8_t b);

	Color operator+(const Color& color2);
};



Color operator*(double scalar, const Color& color);

bool operator==(const Color& color1, const Color& color2);

struct ColorTag {
    Color color;
    float luminance;

	ColorTag();
	ColorTag(const Color& color, float luminance);
};

struct ColorPalette {
	std::string name;
	bool symmetric;

	std::vector<Color> colors;
};

struct Shape {
	ColorPalette palette;
	Color calculate_corresponding_color(float normalized_val /*decimal number ranging from 0 to 1*/);

	float base_luminance;
	float velocity; // rate of translational movement
	int x_offset;
	int y_offset;

	std::vector<double> luminance_weighting_function;

	virtual int get_shape_type();
};

struct TriPrism : public Shape {
	float side, height;
	std::vector<float> side_weighting_function;
	std::vector<float> height_weighting_function;

	int get_shape_type();
};

struct Sphere : public Shape {
	float radius;
	std::vector<double> radius_weighting_function;

	int get_shape_type();
};

struct Donut : public Shape {
	float radius, thickness;
	std::vector<double> radius_weighting_function, thickness_weighting_function;

	int get_shape_type();
};

struct RectPrism : public Shape {
	float height, width, depth;
	std::vector<double> volume_weighting_function;

	int get_shape_type();
};

struct Circle : public Shape {
	float radius;
	std::vector<float> radius_weighting_function;

	int get_shape_type();
};

struct Disc : public Shape {
	float radius, thickness;
	std::vector<float> radius_weighting_function;
	std::vector<float> thickness_weighting_function;

	int get_shape_type();
};

struct Triangle : public Shape {
	float side1, side2, side3;

	int get_shape_type();
};

struct wava_screen {
	const int x, y;
	std::mutex mtx; 

	float K1, K2, R1, R2;

	const std::string background_print_str;
	const std::string shape_print_str;

	const int bg_palette_index;

	const float theta_spacing, phi_spacing, prism_spacing;
	
	ColorPalette bg_palette;

	public:
		static vec3 light;
		const float light_smoothness;

		wava_screen(int x, int y, float theta, float phi, float rect, float smoothness, int palette_index);

		std::vector<double> zbuffer;
		std::vector<ColorTag> output;

		int get_index(int x_coord, int y_coord);

		const char* get_shape_print_str();
		const char* get_background_print_str();

		std::tuple<int, int, float> calculate_proj_coord(vec3 pos);

		void write_to_z_buffer_and_output(const float* zbuffer, const ColorTag* output); // data length is assumed to be correct

};

void draw_donut (Donut donut, wava_screen &screen, std::vector<double> wava_out, float A, float B);

void draw_sphere (Sphere sphere, wava_screen &screen, std::vector<double> wava_out, float A, float B);

void draw_rect_prism (RectPrism rect_prism, wava_screen &screen, std::vector<double> wava_out, float A, float B);

std::vector<Shape*> generate_rand_shapes(int count, float variance, int freq_bands);

ColorPalette generate_rand_palette();

