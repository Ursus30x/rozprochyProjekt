#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>

// Packet IDs
enum PacketType : uint8_t {
    PACKET_CREATE_JOIN_SERVER = 1,
    PACKET_SERVER_RESPONSE = 2,
    PACKET_JOIN_LOBBY = 3,
    PACKET_PLAYER_JOINED = 4,
    PACKET_PLAYER_READY = 5,
    PACKET_READY_PROPAGATION = 6,
    PACKET_GAME_START = 7,
    PACKET_PLAYER_ACTION = 8,
    PACKET_ACTION_PROPAGATION = 9,
    PACKET_GAME_END = 10,
    PACKET_PLAYER_LEAVE = 11,
    PACKET_PLAYER_LEFT = 12,
    PACKET_GAME_SYNC = 13
};

// Akcje graczy
enum PlayerAction : int32_t {
    ACTION_MOVE_LEFT = 0,
    ACTION_MOVE_RIGHT = 1,
    ACTION_STOP = 2
};

// Ściany
enum Wall : int32_t {
    WALL_NORTH = 0,  // Gracz 0 - góra
    WALL_EAST = 1,   // Gracz 1 - prawo
    WALL_SOUTH = 2,  // Gracz 2 - dół
    WALL_WEST = 3    // Gracz 3 - lewo
};

// Struktury komunikatów
struct CreateJoinServerPacket {
    int32_t server_name_length;
    char server_name[64];
};

struct ServerResponsePacket {
    int32_t port;  // -1 oznacza błąd
};

struct JoinLobbyPacket {
    int32_t nick_length;
    char nick[21];  // max 20 + null terminator
};

struct PlayerJoinedPacket {
    int32_t player_id;
    int32_t nick_length;
    char nick[21];
};

struct ReadyPropagationPacket {
    int32_t player_id;
};

struct PlayerActionPacket {
    int32_t action;
};

struct ActionPropagationPacket {
    int32_t action;
    int32_t player_id;
};

struct PlayerScore {
    int32_t player_id;
    int32_t score;
};

struct GameEndPacket {
    int32_t scores_len;
    PlayerScore scores[4];
};

struct PlayerLeftPacket {
    int32_t player_id;
};

struct GameSyncPacket {
    float ball_x;
    float ball_y;
    float ball_velocity_x;
    float ball_velocity_y;
    float paddle_positions[4];
    int32_t scores[4];
};

// Stałe gry
const float ARENA_SIZE = 80.0f;
const float PADDLE_SIZE = 10.0f;
const float BALL_RADIUS = 1.0f;
const float PADDLE_SPEED = 50.0f;
const float BALL_SPEED = 30.0f;
const int INITIAL_SCORE = 5;
const int GAME_FPS = 60;

// Pozycje platform na ścianach
const float PADDLE_OFFSET = 2.0f;

// Klasa Ball
class Ball {
public:
    float x, y;
    float velocity_x, velocity_y;
    float radius;
    
    Ball() : x(ARENA_SIZE/2), y(ARENA_SIZE/2), 
             velocity_x(BALL_SPEED), velocity_y(BALL_SPEED), 
             radius(BALL_RADIUS) {}
    
    void update(float dt) {
        x += velocity_x * dt;
        y += velocity_y * dt;
    }
    
    void bounce_x() { velocity_x = -velocity_x; }
    void bounce_y() { velocity_y = -velocity_y; }
};

// Klasa Paddle
class Paddle {
public:
    float position;  // pozycja wzdłuż ściany
    Wall wall;
    int player_id;
    float size;
    bool moving_left, moving_right;
    
    Paddle(Wall w, int id) : wall(w), player_id(id), size(PADDLE_SIZE), 
                             position(ARENA_SIZE/2), moving_left(false), moving_right(false) {}
    
    void update(float dt) {
        if (moving_left && !moving_right) {
            position -= PADDLE_SPEED * dt;
        } else if (moving_right && !moving_left) {
            position += PADDLE_SPEED * dt;
        }
        
        // Ograniczenia pozycji
        if (position < size/2) position = size/2;
        if (position > ARENA_SIZE - size/2) position = ARENA_SIZE - size/2;
    }
    
    void set_action(PlayerAction action) {
        switch(action) {
            case ACTION_MOVE_LEFT:
                moving_left = true;
                moving_right = false;
                break;
            case ACTION_MOVE_RIGHT:
                moving_left = false;
                moving_right = true;
                break;
            case ACTION_STOP:
                moving_left = false;
                moving_right = false;
                break;
        }
    }
};

// Klasa GameState
class GameState {
public:
    Ball ball;
    std::array<Paddle, 4> paddles;
    std::array<int, 4> scores;
    bool game_running;
    std::array<bool, 4> players_ready;
    int active_players;
    
    GameState() : paddles{Paddle(WALL_NORTH, 0), Paddle(WALL_EAST, 1), 
                          Paddle(WALL_SOUTH, 2), Paddle(WALL_WEST, 3)},
                  game_running(false), active_players(0) {
        for(int i = 0; i < 4; i++) {
            scores[i] = INITIAL_SCORE;
            players_ready[i] = false;
        }
    }
    
    void update(float dt) {
        if (!game_running) return;
        
        // Update paddles
        for (auto& paddle : paddles) {
            paddle.update(dt);
        }
        
        // Update ball
        ball.update(dt);
        
        // Check collisions
        check_collisions();
    }
    
    void check_collisions() {
        // Sprawdzanie kolizji w ustalonej kolejności (zgodnie z dokumentem)
        for (int i = 0; i < 4; i++) {
            if (check_paddle_collision(i)) {
                handle_paddle_bounce(i);
                return;
            }
        }
        
        // Sprawdzanie kolizji ze ścianami
        check_wall_collisions();
    }
    
private:
    bool check_paddle_collision(int paddle_id) {
        const Paddle& paddle = paddles[paddle_id];
        
        switch(paddle.wall) {
            case WALL_NORTH:
                return (ball.y - ball.radius <= PADDLE_OFFSET) &&
                       (ball.x >= paddle.position - paddle.size/2) &&
                       (ball.x <= paddle.position + paddle.size/2);
            case WALL_SOUTH:
                return (ball.y + ball.radius >= ARENA_SIZE - PADDLE_OFFSET) &&
                       (ball.x >= paddle.position - paddle.size/2) &&
                       (ball.x <= paddle.position + paddle.size/2);
            case WALL_WEST:
                return (ball.x - ball.radius <= PADDLE_OFFSET) &&
                       (ball.y >= paddle.position - paddle.size/2) &&
                       (ball.y <= paddle.position + paddle.size/2);
            case WALL_EAST:
                return (ball.x + ball.radius >= ARENA_SIZE - PADDLE_OFFSET) &&
                       (ball.y >= paddle.position - paddle.size/2) &&
                       (ball.y <= paddle.position + paddle.size/2);
        }
        return false;
    }
    
    void handle_paddle_bounce(int paddle_id) {
        const Paddle& paddle = paddles[paddle_id];
        
        // Oblicz kąt odbicia na podstawie miejsca uderzenia
        float hit_pos = 0;
        
        switch(paddle.wall) {
            case WALL_NORTH:
            case WALL_SOUTH:
                hit_pos = (ball.x - paddle.position) / (paddle.size/2);
                ball.velocity_y = -ball.velocity_y;
                ball.velocity_x += hit_pos * BALL_SPEED * 0.5f;
                break;
            case WALL_WEST:
            case WALL_EAST:
                hit_pos = (ball.y - paddle.position) / (paddle.size/2);
                ball.velocity_x = -ball.velocity_x;
                ball.velocity_y += hit_pos * BALL_SPEED * 0.5f;
                break;
        }
        
        // Normalizuj prędkość
        float speed = sqrt(ball.velocity_x * ball.velocity_x + ball.velocity_y * ball.velocity_y);
        if (speed > 0) {
            ball.velocity_x = (ball.velocity_x / speed) * BALL_SPEED;
            ball.velocity_y = (ball.velocity_y / speed) * BALL_SPEED;
        }
    }
    
    void check_wall_collisions() {
        bool scored = false;
        int scoring_player = -1;
        
        // Kolizje ze ścianami
        if (ball.y <= 0) {
            scores[0]--;  // Gracz 0 traci punkt
            scored = true;
        } else if (ball.y >= ARENA_SIZE) {
            scores[2]--;  // Gracz 2 traci punkt
            scored = true;
        } else if (ball.x <= 0) {
            scores[3]--;  // Gracz 3 traci punkt
            scored = true;
        } else if (ball.x >= ARENA_SIZE) {
            scores[1]--;  // Gracz 1 traci punkt
            scored = true;
        }
        
        if (scored) {
            reset_ball();
            check_game_end();
        }
    }
    
    void reset_ball() {
        ball.x = ARENA_SIZE / 2;
        ball.y = ARENA_SIZE / 2;
        ball.velocity_x =  0 ;//(rand() % 2 == 0 ? 1 : -1) * BALL_SPEED;
        ball.velocity_y = (rand() % 2 == 0 ? 1 : -1) * BALL_SPEED;
    }
    
    void check_game_end() {
        int players_alive = 0;
        for (int score : scores) {
            if (score > 0) players_alive++;
        }
        
        if (players_alive <= 1) {
            game_running = false;
        }
    }
};