#include <thread>
#include <cli.h>

void render_cli_frame (std::vector<Shape*> shapes, wava_screen &screen, std::vector<double> wava_out) {
    static int time = 0;
    std::vector<std::thread> threads; 
    std::vector<double> weighting;
    for (int i = 0; i < shapes.size(); i++) {
        switch(shapes[i]->get_shape_type()) {
            case DONUT_SHAPE:
                {
                Donut* donut = (Donut*) shapes[i];

                std::thread donut_thread(draw_donut, *donut, std::ref(screen), wava_out, 0 + time * 0.005, 5 + time * 0.005);
                threads.push_back(std::move(donut_thread));
                }
            break;
            case RECT_PRISM_SHAPE:
                {
                RectPrism* rect_prism = (RectPrism*) shapes[i];
                std::thread rect_prism_thread(draw_rect_prism, *rect_prism, std::ref(screen), weighting, 0 + time * 0.01, 5 + time * 0.01);
                threads.push_back(std::move(rect_prism_thread));
                }
            break;  
            case SPHERE_SHAPE:
                {
                Sphere* sphere = (Sphere*) shapes[i];
                //draw_sphere(*sphere, screen, weighting, 0 + time * 0.01, 5 + time * 0.01);
                std::thread sphere_thread(draw_sphere, *sphere, std::ref(screen), weighting, 0 + time * 0.01, 5 + time * 0.01);
                threads.push_back(std::move(sphere_thread));
                }
            break;
            default:
                {
                }
            break;
        }
    }

    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    printf("\x1b[H"); // brings cursor to beginning of terminal window
    for (int x = 0; x < screen.x_dim(); ++x) {
        for (int y = 0; y < screen.y_dim(); ++y) {
            int curr_index = screen.get_index(x, y);

            ColorTag curr_tag = screen.output[curr_index];
            float luminance = curr_tag.luminance;
            Color color = curr_tag.color;

            if (screen.light_smoothness != -1) {
                luminance *= screen.light_smoothness;
                //printf("luminance is: %f", luminance);
                int temp = (int) luminance;
                //printf("temp is %d", temp);
                luminance = (float) (temp/screen.light_smoothness);
                //printf("luminance is %f\n", luminance);
            }

            if (luminance <= 0 && (color == Color(0, 0, 0))) { 
                int largest = 0;
                for (int i = 0; i < 12; i++) {
                    if (wava_out[i + 3] > wava_out[largest]) largest = i; 
                    //color = wava_out[i + 3] * screen.bg_palette.colors[i % screen.bg_palette.colors.size()] + color;
                }
                color = wava_out[largest + 3] * 2 * screen.bg_palette.colors[largest % screen.bg_palette.colors.size()];
                //for (int i = 0; i < screen.bg_palette.colors.size(); i++) {
                //    color = wava_out[i + 3] * screen.bg_palette.colors[i] + color;
                //}
            }
            
            if (luminance > 0) 
                printf (
                    "\x1b[38;2;%d;%d;%dm%s", 
                    (int) (luminance * color.r), 
                    (int) (luminance * color.g), 
                    (int) (luminance * color.b), 
                    screen.get_shape_print_str()
                );
            else               
                printf (
                    "\x1b[38;2;%d;%d;%dm%s", 
                    (int) (1 * color.r), 
                    (int) (1 * color.g), 
                    (int) (1 * color.b), 
                    screen.get_background_print_str()
                );

            screen.zbuffer[curr_index] = 0;   // put this in same loop to increase performance
            screen.output[curr_index].luminance = 0;
            screen.output[curr_index].color = Color(0,0,0);

        }
        printf ("\n");
    }
    time++;
}