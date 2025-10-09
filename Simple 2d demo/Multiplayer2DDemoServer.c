#define GL_SILENCE_DEPRECATION
#include <stdio.h>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX  1024

typedef struct {
    int id;      // simple numeric ID
    int fd;      // socket descriptor
    int active;
} client_t;

// Grid size
#define GRID_SIZE 16

// Player buffer
int players[4][2] = {{14,14}, {1,1}};
float colors[4][3] = {{0.98f, 0.73f, 0.01f},{0.19f, 0.89f, 0.75f}};

// Callback for window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Handle input (ESC to close)
void processInput(GLFWwindow* window) {
    const double moveDelay = 0.15; // seconds between moves while holding
    double playercooldown = 0;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);

    double t = glfwGetTime();
    if (t-playercooldown < moveDelay) {return;}

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        if (players[0][1] < GRID_SIZE-1) { players[0][1]++; playercooldown = t; }
    }
    else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        if (players[0][1] > 0) { players[0][1]--; playercooldown = t; }
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        if (players[0][0] > 0) { players[0][0]--; playercooldown = t; }
    }
    else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        if (players[0][0] < GRID_SIZE-1) { players[0][0]++; playercooldown = t; }
    }
    // Do a tcp-sync packet here... basically something sending the movement to the server... or if the server, process the packet and send to all clients...
    // Packet structure: 2 bits - client id (0=server), 5 bits - x coord, 5 bits - y coord
}

// Vertex and fragment shader sources (inline for simplicity)
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 offset;\n"
"uniform float scale;\n"
"void main() {\n"
"    vec2 p = aPos * scale + offset;\n" 
"    gl_Position = vec4(p, 0.0, 1.0);\n"
"}\n";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec3 color;\n"
"void main() {\n"
"    FragColor = vec4(color, 1.0);\n"
"}\n";

int main() {
    // Setup TCP socket to listn for client connections
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listenfd, 10) < 0) { perror("listen"); exit(1); }

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(listenfd, &master);
    FD_SET(STDIN_FILENO, &master);
    int fdmax = (listenfd > STDIN_FILENO) ? listenfd : STDIN_FILENO;

    client_t clients[FD_SETSIZE];
    for (int i = 0; i < FD_SETSIZE; i++) clients[i].active = 0;

    int next_id = 1;

    printf("Server listening on port %d\n", PORT);
    printf("Commands from server console:\n");
    printf("   <id> <message>   send message to a client\n");
    printf("   exit             shut down server (sends exit to all)\n");


    // Initialize GLFW
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }

    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(600, 600, "Grid Demo", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Load GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    // set initial viewport (very important)
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    // Define rectangle vertices (two triangles)
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    // Create VAO & VBO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Link shader program
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);
    // checkLink(prog);

    // Uniform locations
    GLint locOffset = glGetUniformLocation(prog, "offset");
    GLint locScale  = glGetUniformLocation(prog, "scale");
    GLint locColor  = glGetUniformLocation(prog, "color");

    float cellScale = 2.0f / GRID_SIZE; // scale square to fit grid

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glUseProgram(prog);
        glBindVertexArray(VAO);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (locOffset == -1) fprintf(stderr, "Warning: 'offset' uniform not found\n");

        // draw grid cells (light gray)
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float ox = -1.0f + cellScale * (x + 0.5f);
                float oy = -1.0f + cellScale * (y + 0.5f);
                glUniform2f(locOffset, ox, oy);
                glUniform1f(locScale, cellScale);
                glUniform3f(locColor, 0.25f, 0.25f, 0.25f);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // draw player (green)
        for (int i = 0; i < sizeof(players)/sizeof(players[0]); i++){
            float pox = -1.0f + cellScale * (players[i][0] + 0.5f);
            float poy = -1.0f + cellScale * (players[i][1] + 0.5f);
            glUniform2f(locOffset, pox, poy);
            glUniform1f(locScale, cellScale * 0.9f); // slightly smaller so grid lines show
            float* color = colors[i];
            glUniform3f(locColor, color[0], color[1], color[2]);
            glDrawArrays(GL_TRIANGLES, 0, 6);

        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwTerminate();
    return 0;
}
