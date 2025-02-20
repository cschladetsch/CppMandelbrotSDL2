#include <SDL2/SDL.h>
#include <complex>
#include <iostream>
#include <thread>
#include <vector>
#include <cmath>

class MandelbrotExplorer {
private:
    static constexpr int WINDOW_WIDTH = 800;
    static constexpr int WINDOW_HEIGHT = 600;
    static constexpr int MAX_ITERATIONS = 1000;
    
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    std::vector<uint32_t> pixels;
    std::vector<uint32_t> tempPixels;
    
    // View parameters
    double centerX = -0.5;
    double centerY = 0.0;
    double zoom = 1.0;
    
    SDL_Point dragStart{};
    bool isDragging = false;

    uint32_t getColor(int iterations) const {
        if (iterations == MAX_ITERATIONS) return 0;
        
        // Smooth coloring
        double smooth = iterations + 1 - std::log2(std::log2(std::abs(iterations)));
        double hue = std::fmod(smooth / 32.0, 1.0);
        
        // Better color distribution
        double r = std::abs(std::sin(2 * M_PI * (hue + 0.0/3.0)));
        double g = std::abs(std::sin(2 * M_PI * (hue + 1.0/3.0)));
        double b = std::abs(std::sin(2 * M_PI * (hue + 2.0/3.0)));
        
        return static_cast<uint32_t>(r * 255) << 16 |
               static_cast<uint32_t>(g * 255) << 8 |
               static_cast<uint32_t>(b * 255);
    }

    int calculateMandelbrot(std::complex<double> c) const {
        std::complex<double> z = 0;
        int iterations = 0;
        double zabs;
        
        while ((zabs = std::abs(z)) <= 2.0 && iterations < MAX_ITERATIONS) {
            if (zabs > 2.0) break;
            z = z * z + c;
            iterations++;
        }
        
        return iterations;
    }

    void renderMandelbrot() {
        const int numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        std::vector<std::vector<uint32_t>> threadBuffers(numThreads, std::vector<uint32_t>(WINDOW_WIDTH * WINDOW_HEIGHT));
        
        auto renderLine = [this](std::vector<uint32_t>& buffer, int startY, int endY) {
            for (int y = startY; y < endY; ++y) {
                for (int x = 0; x < WINDOW_WIDTH; ++x) {
                    double real = (x - WINDOW_WIDTH/2.0) / (zoom * WINDOW_WIDTH/4.0) + centerX;
                    double imag = (y - WINDOW_HEIGHT/2.0) / (zoom * WINDOW_WIDTH/4.0) + centerY;
                    
                    std::complex<double> c(real, imag);
                    int iterations = calculateMandelbrot(c);
                    buffer[y * WINDOW_WIDTH + x] = getColor(iterations);
                }
            }
        };
        
        // Split work among threads with separate buffers
        int linesPerThread = WINDOW_HEIGHT / numThreads;
        for (int i = 0; i < numThreads; ++i) {
            int startY = i * linesPerThread;
            int endY = (i == numThreads - 1) ? WINDOW_HEIGHT : (i + 1) * linesPerThread;
            threads.emplace_back(renderLine, std::ref(threadBuffers[i]), startY, endY);
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Combine thread buffers into final image
        for (int i = 0; i < numThreads; ++i) {
            int startY = i * linesPerThread;
            int endY = (i == numThreads - 1) ? WINDOW_HEIGHT : (i + 1) * linesPerThread;
            int size = (endY - startY) * WINDOW_WIDTH;
            std::copy(
                threadBuffers[i].begin() + startY * WINDOW_WIDTH,
                threadBuffers[i].begin() + endY * WINDOW_WIDTH,
                tempPixels.begin() + startY * WINDOW_WIDTH
            );
        }
        
        // Atomic buffer swap and render
        std::swap(pixels, tempPixels);
        SDL_UpdateTexture(texture, nullptr, pixels.data(), WINDOW_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

public:
    MandelbrotExplorer() 
        : pixels(WINDOW_WIDTH * WINDOW_HEIGHT)
        , tempPixels(WINDOW_WIDTH * WINDOW_HEIGHT) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
        }
        
        window = SDL_CreateWindow("Mandelbrot Explorer",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                WINDOW_WIDTH, WINDOW_HEIGHT,
                                SDL_WINDOW_SHOWN);
        
        if (!window) {
            throw std::runtime_error("Window creation failed");
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            throw std::runtime_error(std::string("Hardware renderer creation failed: ") + SDL_GetError());
        }

        texture = SDL_CreateTexture(renderer,
                                  SDL_PIXELFORMAT_RGB888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!texture) {
            throw std::runtime_error("Texture creation failed");
        }
    }
    
    ~MandelbrotExplorer() {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    void run() {
        bool running = true;
        SDL_Event event;
        
        renderMandelbrot();
        
        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                            running = false;
                        }
                        break;
                        
                    case SDL_KEYDOWN:
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            running = false;
                        }
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            dragStart = {event.button.x, event.button.y};
                            isDragging = true;
                        }
                        break;
                        
                    case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            isDragging = false;
                        }
                        break;
                        
                    case SDL_MOUSEMOTION:
                        if (isDragging) {
                            double dx = (event.motion.x - dragStart.x) / (zoom * WINDOW_WIDTH/4.0);
                            double dy = (event.motion.y - dragStart.y) / (zoom * WINDOW_WIDTH/4.0);
                            centerX -= dx;
                            centerY -= dy;
                            dragStart = {event.motion.x, event.motion.y};
                            renderMandelbrot();
                        }
                        break;
                        
                    case SDL_MOUSEWHEEL:
                        {
                            int mouseX, mouseY;
                            SDL_GetMouseState(&mouseX, &mouseY);
                            
                            double mouseWorldX = (mouseX - WINDOW_WIDTH/2.0) / (zoom * WINDOW_WIDTH/4.0) + centerX;
                            double mouseWorldY = (mouseY - WINDOW_HEIGHT/2.0) / (zoom * WINDOW_WIDTH/4.0) + centerY;
                            
                            // Apply zooming in smaller steps for smoothness
                            double targetZoom = event.wheel.y > 0 ? zoom * 1.1 : zoom / 1.1;
                            double steps = 1;
                            double currentZoom = zoom;
                            
                            while (std::abs(currentZoom - targetZoom) > 0.0001) {
                                currentZoom = zoom + (targetZoom - zoom) * (steps / 10.0);
                                zoom = currentZoom;
                                
                                centerX = mouseWorldX - (mouseX - WINDOW_WIDTH/2.0) / (zoom * WINDOW_WIDTH/4.0);
                                centerY = mouseWorldY - (mouseY - WINDOW_HEIGHT/2.0) / (zoom * WINDOW_WIDTH/4.0);
                                
                                renderMandelbrot();
                                steps++;
                                if (steps > 10) break; // Ensure we don't loop forever
                            }
                        }
                        break;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        MandelbrotExplorer().run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
