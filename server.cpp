#include "common.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>

struct PlayerConnection {
    int tcp_socket;
    int udp_socket;
    sockaddr_in udp_addr;
    std::string nick;
    bool connected;
    bool ready;
    
    PlayerConnection() : tcp_socket(-1), udp_socket(-1), connected(false), ready(false) {}
};

struct ActionEvent {
    int player_id;
    PlayerAction action;
    std::chrono::steady_clock::time_point timestamp;
};

class GameServer {
private:
    GameState game_state;
    std::array<PlayerConnection, 4> players;
    std::mutex game_mutex;
    std::queue<ActionEvent> action_queue;
    std::mutex queue_mutex;
    int server_socket;
    int udp_socket;
    bool running;
    std::thread game_thread;
    
public:
    GameServer() : running(false), server_socket(-1), udp_socket(-1) {}
    
    ~GameServer() {
        stop();
    }
    
    bool start(int port) {
        // Tworzenie TCP socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            std::cerr << "Błąd tworzenia TCP socket\n";
            return false;
        }
        
        // Tworzenie UDP socket
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0) {
            std::cerr << "Błąd tworzenia UDP socket\n";
            close(server_socket);
            return false;
        }
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Błąd bind TCP socket\n";
            close(server_socket);
            close(udp_socket);
            return false;
        }
        
        if (bind(udp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Błąd bind UDP socket\n";
            close(server_socket);
            close(udp_socket);
            return false;
        }
        
        if (listen(server_socket, 4) < 0) {
            std::cerr << "Błąd listen\n";
            close(server_socket);
            close(udp_socket);
            return false;
        }
        
        running = true;
        game_thread = std::thread(&GameServer::game_loop, this);
        
        std::cout << "Serwer uruchomiony na porcie " << port << std::endl;
        return true;
    }
    
    void stop() {
        running = false;
        if (game_thread.joinable()) {
            game_thread.join();
        }
        
        for (auto& player : players) {
            if (player.tcp_socket >= 0) {
                close(player.tcp_socket);
            }
        }
        
        if (server_socket >= 0) close(server_socket);
        if (udp_socket >= 0) close(udp_socket);
    }
    
    void accept_connections() {
        while (running) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
            if (client_socket < 0) continue;
            
            // Znajdź wolne miejsce dla gracza
            int player_id = -1;
            for (int i = 0; i < 4; i++) {
                if (!players[i].connected) {
                    player_id = i;
                    break;
                }
            }
            
            if (player_id == -1) {
                close(client_socket);
                continue;
            }
            
            // Obsłuż dołączenie gracza
            handle_player_join(client_socket, player_id, client_addr);
        }
    }
    
private:
    void handle_player_join(int socket, int player_id, sockaddr_in addr) {
        // Odbierz nick gracza
        uint8_t packet_type;
        recv(socket, &packet_type, 1, 0);
        
        if (packet_type != PACKET_JOIN_LOBBY) {
            close(socket);
            return;
        }
        
        JoinLobbyPacket join_packet;
        recv(socket, &join_packet, sizeof(join_packet), 0);
        
        // Ustaw gracza
        players[player_id].tcp_socket = socket;
        players[player_id].udp_socket = udp_socket;
        players[player_id].udp_addr = addr;
        players[player_id].nick = std::string(join_packet.nick, join_packet.nick_length);
        players[player_id].connected = true;
        
        // Wyślij potwierdzenie
        uint8_t response_type = PACKET_PLAYER_JOINED;
        send(socket, &response_type, 1, 0);
        
        PlayerJoinedPacket response;
        response.player_id = player_id;
        response.nick_length = join_packet.nick_length;
        strcpy(response.nick, join_packet.nick);
        
        send(socket, &response, sizeof(response), 0);
        
        // Uruchom wątek obsługi gracza
        std::thread(&GameServer::handle_player, this, player_id).detach();
        
        std::cout << "Gracz " << player_id << " (" << players[player_id].nick << ") dołączył\n";
    }
    
    void handle_player(int player_id) {
        int socket = players[player_id].tcp_socket;
        
        while (running && players[player_id].connected) {
            uint8_t packet_type;
            int bytes = recv(socket, &packet_type, 1, MSG_DONTWAIT);
            
            if (bytes <= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            switch (packet_type) {
                case PACKET_PLAYER_READY:
                    handle_player_ready(player_id);
                    break;
                case PACKET_PLAYER_LEAVE:
                    handle_player_leave(player_id);
                    return;
            }
        }
    }
    
    void handle_player_ready(int player_id) {
        players[player_id].ready = true;
        
        // Powiadom innych graczy
        uint8_t packet_type = PACKET_READY_PROPAGATION;
        ReadyPropagationPacket packet;
        packet.player_id = player_id;
        
        for (int i = 0; i < 4; i++) {
            if (i != player_id && players[i].connected) {
                send(players[i].tcp_socket, &packet_type, 1, 0);
                send(players[i].tcp_socket, &packet, sizeof(packet), 0);
            }
        }
        
        // Sprawdź czy wszyscy gotowi
        bool all_ready = true;
        int connected_players = 0;
        for (int i = 0; i < 4; i++) {
            if (players[i].connected) {
                connected_players++;
                if (!players[i].ready) {
                    all_ready = false;
                }
            }
        }
        
        if (all_ready && connected_players == 4) {
            start_game();
        }
    }
    
    void start_game() {
        std::lock_guard<std::mutex> lock(game_mutex);
        game_state.game_running = true;
        game_state.active_players = 4;
        
        // Powiadom graczy o rozpoczęciu gry
        uint8_t packet_type = PACKET_GAME_START;
        for (int i = 0; i < 4; i++) {
            if (players[i].connected) {
                send(players[i].tcp_socket, &packet_type, 1, 0);
            }
        }
        
        std::cout << "Gra rozpoczęta!\n";
    }
    
    void handle_player_leave(int player_id) {
        players[player_id].connected = false;
        players[player_id].ready = false;
        close(players[player_id].tcp_socket);
        
        // Powiadom innych graczy
        uint8_t packet_type = PACKET_PLAYER_LEFT;
        PlayerLeftPacket packet;
        packet.player_id = player_id;
        
        for (int i = 0; i < 4; i++) {
            if (i != player_id && players[i].connected) {
                send(players[i].tcp_socket, &packet_type, 1, 0);
                send(players[i].tcp_socket, &packet, sizeof(packet), 0);
            }
        }
        
        std::cout << "Gracz " << player_id << " opuścił grę\n";
    }
    
    void handle_udp_messages() {
        char buffer[1024];
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        while (running) {
            int bytes = recvfrom(udp_socket, buffer, sizeof(buffer), MSG_DONTWAIT, 
                               (sockaddr*)&client_addr, &addr_len);
            
            if (bytes <= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            if (bytes < sizeof(uint8_t) + sizeof(PlayerActionPacket)) continue;
            
            uint8_t packet_type = buffer[0];
            if (packet_type != PACKET_PLAYER_ACTION) continue;
            
            PlayerActionPacket* action_packet = (PlayerActionPacket*)(buffer + 1);
            
            // Znajdź gracza po adresie
            int player_id = -1;
            for (int i = 0; i < 4; i++) {
                if (players[i].connected && 
                    players[i].udp_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                    players[i].udp_addr.sin_port == client_addr.sin_port) {
                    player_id = i;
                    break;
                }
            }
            
            if (player_id == -1) continue;
            
            // Dodaj akcję do kolejki
            ActionEvent event;
            event.player_id = player_id;
            event.action = (PlayerAction)action_packet->action;
            event.timestamp = std::chrono::steady_clock::now();
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                action_queue.push(event);
            }
            
            // Propaguj akcję do innych graczy
            propagate_action(player_id, event.action);
        }
    }
    
    void propagate_action(int player_id, PlayerAction action) {
        char buffer[sizeof(uint8_t) + sizeof(ActionPropagationPacket)];
        buffer[0] = PACKET_ACTION_PROPAGATION;
        
        ActionPropagationPacket* packet = (ActionPropagationPacket*)(buffer + 1);
        packet->player_id = player_id;
        packet->action = action;
        
        for (int i = 0; i < 4; i++) {
            if (i != player_id && players[i].connected) {
                sendto(udp_socket, buffer, sizeof(buffer), 0, 
                       (sockaddr*)&players[i].udp_addr, sizeof(players[i].udp_addr));
            }
        }
    }
    
    void game_loop() {
        auto last_time = std::chrono::steady_clock::now();
        auto last_sync = last_time;
        
        // Uruchom obsługę UDP w osobnym wątku
        std::thread udp_thread(&GameServer::handle_udp_messages, this);
        
        while (running) {
            auto current_time = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;
            
            // Przetwórz akcje z kolejki
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                while (!action_queue.empty()) {
                    ActionEvent event = action_queue.front();
                    action_queue.pop();
                    
                    if (game_state.game_running) {
                        game_state.paddles[event.player_id].set_action(event.action);
                    }
                }
            }
            
            // Aktualizuj stan gry
            {
                std::lock_guard<std::mutex> lock(game_mutex);
                game_state.update(dt);
            }
            
            // Synchronizacja co 100ms
            if (std::chrono::duration<float>(current_time - last_sync).count() > 0.1f) {
                sync_game_state();
                last_sync = current_time;
            }
            
            // 60 FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        if (udp_thread.joinable()) {
            udp_thread.join();
        }
    }
    
    void sync_game_state() {
        if (!game_state.game_running) return;
        
        char buffer[sizeof(uint8_t) + sizeof(GameSyncPacket)];
        buffer[0] = PACKET_GAME_SYNC;
        
        GameSyncPacket* sync_packet = (GameSyncPacket*)(buffer + 1);
        sync_packet->ball_x = game_state.ball.x;
        sync_packet->ball_y = game_state.ball.y;
        sync_packet->ball_velocity_x = game_state.ball.velocity_x;
        sync_packet->ball_velocity_y = game_state.ball.velocity_y;
        
        for (int i = 0; i < 4; i++) {
            sync_packet->paddle_positions[i] = game_state.paddles[i].position;
            sync_packet->scores[i] = game_state.scores[i];
        }
        
        for (int i = 0; i < 4; i++) {
            if (players[i].connected) {
                sendto(udp_socket, buffer, sizeof(buffer), 0,
                       (sockaddr*)&players[i].udp_addr, sizeof(players[i].udp_addr));
            }
        }
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    GameServer server;
    if (!server.start(port)) {
        return 1;
    }
    
    server.accept_connections();
    
    return 0;
}