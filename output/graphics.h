#ifndef WAVA_GRAPHICS
#define WAVA_GRAPHICS

#include <vector>
#include <mutex>

// color/shapes portion
#define RECT_PRISM_SHAPE 0
#define SPHERE_SHAPE 1
#define DONUT_SHAPE 2
#define TRI_PRISM_SHAPE 3
#define CIRCLE_SHAPE 4
#define DISC_SHAPE 5
#define TRIANGLE_SHAPE 6
#define WAVA_SHAPE_COUNT 7

#define PRIDE_FLAG_PALETTE 0
#define TRANS_FLAG_PALETTE 1
#define WAVA_PALETTE_COUNT 2

#define BASS_WEIGHT_FUNCTION 0
#define MID_WEIGHT_FUNCTION 1
#define TREBLE_WEIGHT_FUNCTION 2
#define WAVA_WEIGHT_FUNCTION_COUNT 3

// geometry/matrix portion
#ifndef PI
#define PI 3.14159265359
#endif
#define THETA_SPACING 0.041
#define PHI_SPACING 0.041
#define PRISM_SPACING 0.041

struct vecx // dynamic length vector, used for weighting functions
{
	std::vector<float> components;

	void normalize();

	float operator*(const vecx vec);

	vecx();
	vecx(std::vector<float>);

	void destroy();
};

struct vec3
{
    float x, y, z;

    vec3 operator+(const vec3& vec);
    vec3 operator-(const vec3& vec);

    vec3 operator*(const float& scalar);
    vec3 operator/(const float& scalar);

    float operator*(const vec3& vec);

    vec3 operator^(const vec3& vec);

    float magnitude (); 

    void normalize();

    void print_coord (); // debugging
};

struct wava_screen {
	int x_dim; int y_dim;
	double* zbuffer;
	ColorTag* output;
	std::mutex mtx;

	static vec3 light;

	static float light_smoothness;

	wava_screen(int x, int y);

	int get_index(int x_coord, int y_coord);

	void write_to_z_buffer_and_output(const float* zbuf_data, const ColorTag* output_data); // data length is assumed to be correct

	void destroy();
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

void draw_donut (const Donut donut, wava_screen* screen, const std::vector<float> weighting_coefficients, const float A, const float B);

void draw_sphere (const Sphere sphere, wava_screen* screen, const std::vector<float> weighting_coefficients, const float A, const float B);

void draw_rect_prism (const RectPrism rect_prism, wava_screen* screen, const std::vector<float> weighting_coefficients, const float A, const float B);


// shapes/colors portion
struct Color {
	uint8_t r; uint8_t g; uint8_t b;

	Color();
	Color(uint8_t r, uint8_t g, uint8_t b);
};

struct ColorTag {
    Color color;
    float luminance;

	ColorTag();
	ColorTag(Color color, float luminance);
};

struct ColorPalette {
	std::string name;
	std::vector<Color> colors;

	bool symmetric;
};

struct Shape {
	ColorPalette palette;

	float base_luminance;
	float velocity; // rate of translational movement
	int x_offset;
	int y_offset;

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
	std::vector<float> radius_weighting_function;

	int get_shape_type();
};

struct Donut : public Shape {
	float radius, thickness;
	vecx radius_weighting_function, thickness_weighting_function;

	int get_shape_type();
};

struct RectPrism : public Shape {
	float height, width, depth;
	std::vector<float> length_weighting_function;
	std::vector<float> width_weighting_function;
	std::vector<float> depth_weighting_function;

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

std::vector<Shape*> generate_rand_shapes(int count, float variance, int freq_bands);

#endif
