#include "common.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <ncurses.h>

void logToFile(const std::string& message) {
    std::ofstream logFile("log_client.txt", std::ios::app); // tryb dopisywania (append)
    if (logFile.is_open()) {
        logFile << "[LOG]" << message << std::endl;
    } else {
        std::cerr << "Nie można otworzyć pliku log.txt!" << std::endl;
    }
}

class GameClient {
private:
    GameState game_state;
    int tcp_socket;
    int udp_socket;
    sockaddr_in server_addr;
    int my_player_id;
    bool connected;
    bool game_active;
    std::mutex state_mutex;
    std::thread network_thread;
    std::thread input_thread;
    
public:
    GameClient() : tcp_socket(-1), udp_socket(-1), my_player_id(-1), 
                   connected(false), game_active(false) {}
    
    ~GameClient() {
        disconnect();
    }
    
      
    bool connect_to_server(const std::string& ip, int port) {
        // Tworzenie socketów
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        
        if (tcp_socket < 0 || udp_socket < 0) {
            std::cerr << "Błąd tworzenia socketów\n";
            return false;
        }
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
        
        if (connect(tcp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Błąd połączenia z serwerem\n";
            close(tcp_socket);
            close(udp_socket);
            return false;
        }
        
        // POPRAWKA: Bind UDP socket do dowolnego portu
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port = 0; // System wybierze wolny port
        
        if (bind(udp_socket, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            std::cerr << "Błąd bind UDP socket\n";
            close(tcp_socket);
            close(udp_socket);
            return false;
        }
        
        connected = true;
        return true;
    }
    
    
    bool join_lobby(const std::string& nick) {
        if (!connected) return false;
        
        uint8_t packet_type = PACKET_JOIN_LOBBY;
        send(tcp_socket, &packet_type, 1, 0);
        
        JoinLobbyPacket join_packet;
        join_packet.nick_length = nick.length();
        strncpy(join_packet.nick, nick.c_str(), sizeof(join_packet.nick) - 1);
        join_packet.nick[sizeof(join_packet.nick) - 1] = '\0';
        
        send(tcp_socket, &join_packet, sizeof(join_packet), 0);
        
        // Odbierz potwierdzenie
        uint8_t response_type;
        recv(tcp_socket, &response_type, 1, 0);
        
        if (response_type != PACKET_PLAYER_JOINED) {
            return false;
        }
        
        PlayerJoinedPacket response;
        recv(tcp_socket, &response, sizeof(response), 0);
        my_player_id = response.player_id;
        
        std::cout << "Dołączono jako gracz " << my_player_id << std::endl;
        
        // Uruchom wątki
        network_thread = std::thread(&GameClient::network_loop, this);
        input_thread = std::thread(&GameClient::input_loop, this);
        
        return true;
    }
    
    void set_ready() {
        if (!connected) return;
        
        uint8_t packet_type = PACKET_PLAYER_READY;
        send(tcp_socket, &packet_type, 1, 0);
        
        std::cout << "Zaznaczono gotowość. Oczekiwanie na innych graczy...\n";
    }
    
    void disconnect() {
        if (connected) {
            uint8_t packet_type = PACKET_PLAYER_LEAVE;
            send(tcp_socket, &packet_type, 1, 0);
            
            connected = false;
            game_active = false;
            
            if (network_thread.joinable()) network_thread.join();
            if (input_thread.joinable()) input_thread.join();
            
            close(tcp_socket);
            close(udp_socket);
        }
    }
    
    void wait_for_game() {
        while (connected && !game_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (game_active) {
            game_loop();
        }
    }
    
private:
    void network_loop() {
        while (connected) {
            uint8_t packet_type;
            int bytes = recv(tcp_socket, &packet_type, 1, MSG_DONTWAIT);
            
            if (bytes <= 0) {
                // Sprawdź UDP
                handle_udp_messages();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            switch (packet_type) {
                case PACKET_READY_PROPAGATION:
                    handle_ready_propagation();
                    break;
                case PACKET_GAME_START:
                    handle_game_start();
                    break;
                case PACKET_GAME_END:
                    handle_game_end();
                    break;
                case PACKET_PLAYER_LEFT:
                    handle_player_left();
                    break;
            }

            logToFile("dostal pakiet");
        }
    }
    
    void handle_udp_messages() {
        char buffer[1024];
        sockaddr_in from_addr;
        socklen_t addr_len = sizeof(from_addr);
        
        int bytes = recvfrom(udp_socket, buffer, sizeof(buffer), MSG_DONTWAIT,
                           (sockaddr*)&from_addr, &addr_len);
        
        if (bytes <= 0) return;
        
        // POPRAWKA: Dodaj więcej logowania
        logToFile("Otrzymano wiadomość UDP, rozmiar: " + std::to_string(bytes));
        
        if (bytes < sizeof(uint8_t)) {
            logToFile("Za mała wiadomość UDP");
            return;
        }
        
        uint8_t packet_type = buffer[0];
        logToFile("Typ pakietu UDP: " + std::to_string((int)packet_type));
        
        switch (packet_type) {
            case PACKET_ACTION_PROPAGATION:
                if (bytes >= sizeof(uint8_t) + sizeof(ActionPropagationPacket)) {
                    handle_action_propagation((ActionPropagationPacket*)(buffer + 1));
                    logToFile("Obsłużono propagację akcji");
                } else {
                    logToFile("Za mały pakiet dla propagacji akcji");
                }
                break;
            case PACKET_GAME_SYNC:
                if (bytes >= sizeof(uint8_t) + sizeof(GameSyncPacket)) {
                    logToFile("Handluje game sync");
                    handle_game_sync((GameSyncPacket*)(buffer + 1));
                } else {
                    logToFile("Za mały pakiet dla game sync");
                }
                break;
            default:
                logToFile("Nieznany typ pakietu UDP: " + std::to_string((int)packet_type));
                break;
        }
    }
    
    void handle_ready_propagation() {
        ReadyPropagationPacket packet;
        recv(tcp_socket, &packet, sizeof(packet), 0);
        
        std::cout << "Gracz " << packet.player_id << " jest gotowy\n";
    }
    
    void handle_game_start() {
        std::lock_guard<std::mutex> lock(state_mutex);
        game_active = true;
        game_state.game_running = true;
        
        std::cout << "Gra rozpoczęta!\n";
        std::cout << "Sterowanie: strzałki lewo/prawo lub A/D\n";
        std::cout << "Naciśnij SPACE aby być gotowym do wyjścia\n";
    }
    
    void handle_action_propagation(ActionPropagationPacket* packet) {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (game_state.game_running && packet->player_id != my_player_id) {
            game_state.paddles[packet->player_id].set_action((PlayerAction)packet->action);
        }
    }
    
    void handle_game_sync(GameSyncPacket* packet) {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        logToFile("pozycja pilki:" + std::to_string(packet->ball_x) + " " + std::to_string(packet->ball_y));
        game_state.ball.x = packet->ball_x;
        game_state.ball.y = packet->ball_y;
        game_state.ball.velocity_x = packet->ball_velocity_x;
        game_state.ball.velocity_y = packet->ball_velocity_y;
        
        for (int i = 0; i < 4; i++) {
            game_state.paddles[i].position = packet->paddle_positions[i];
            game_state.scores[i] = packet->scores[i];
        }
    }
    
    void handle_game_end() {
        GameEndPacket packet;
        recv(tcp_socket, &packet, sizeof(packet), 0);
        
        std::lock_guard<std::mutex> lock(state_mutex);
        game_active = false;
        game_state.game_running = false;
        
        std::cout << "\n=== KONIEC GRY ===\n";
        for (int i = 0; i < packet.scores_len; i++) {
            std::cout << "Gracz " << packet.scores[i].player_id 
                      << ": " << packet.scores[i].score << " punktów\n";
        }
    }
    
    void handle_player_left() {
        PlayerLeftPacket packet;
        recv(tcp_socket, &packet, sizeof(packet), 0);
        
        std::cout << "Gracz " << packet.player_id << " opuścił grę\n";
    }
    
    void send_action(PlayerAction action) {
        char buffer[sizeof(uint8_t) + sizeof(PlayerActionPacket)];
        buffer[0] = PACKET_PLAYER_ACTION;
        
        PlayerActionPacket* packet = (PlayerActionPacket*)(buffer + 1);
        packet->action = action;
        
        // POPRAWKA: Dodaj logowanie do debugowania
        std::cout << "Wysyłanie akcji: " << (int)action << std::endl;
        logToFile("Wysyłanie akcji UDP: " + std::to_string((int)action));
        
        int result = sendto(udp_socket, buffer, sizeof(buffer), 0,
                           (sockaddr*)&server_addr, sizeof(server_addr));
        
        if (result < 0) {
            std::cerr << "Błąd wysyłania UDP: " << strerror(errno) << std::endl;
            logToFile("Błąd wysyłania UDP: " + std::string(strerror(errno)));
        }
    }
    
    void input_loop() {
        // Inicjalizacja ncurses
        initscr();
        cbreak();
        noecho();
        nodelay(stdscr, TRUE);
        keypad(stdscr, TRUE);
        curs_set(0);
        
        // Inicjalizacja kolorów
        if (has_colors()) {
            start_color();
            init_pair(1, COLOR_RED, COLOR_BLACK);     // Gracz
            init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Inne platformy
            init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Kulka
            init_pair(4, COLOR_CYAN, COLOR_BLACK);    // Ramka
            init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Wyniki
        }
        
        bool left_pressed = false, right_pressed = false;
        
        while (connected) {
            int ch = getch();
            
            if (ch != ERR && game_active) {
                PlayerAction action = ACTION_STOP;
                bool send_update = false;
                
                switch (ch) {
                    case 'a':
                    case 'A':
                    case KEY_LEFT:
                        if (!left_pressed) {
                            left_pressed = true;
                            right_pressed = false;
                            action = ACTION_MOVE_LEFT;
                            send_update = true;
                        }
                        break;
                    case 'd':
                    case 'D':
                    case KEY_RIGHT:
                        if (!right_pressed) {
                            right_pressed = true;
                            left_pressed = false;
                            action = ACTION_MOVE_RIGHT;
                            send_update = true;
                        }
                        break;
                    case ' ':
                        if (left_pressed || right_pressed) {
                            left_pressed = false;
                            right_pressed = false;
                            action = ACTION_STOP;
                            send_update = true;
                        }
                        break;
                    case 27: // ESC
                    case 'q':
                    case 'Q':
                        connected = false;
                        break;
                }
                
                if (send_update) {
                    send_action(action);
                    
                    // Zaktualizuj lokalnie
                    std::lock_guard<std::mutex> lock(state_mutex);
                    game_state.paddles[my_player_id].set_action(action);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        // Przywróć terminal
        endwin();
    }
    
    void game_loop() {
        auto last_time = std::chrono::steady_clock::now();
        
        while (connected && game_active) {
            auto current_time = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;
            
            // Aktualizuj lokalny stan
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                game_state.update(dt);
            }
            
            // Wyrenderuj grę
            render_game();
            
            // 60 FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    
    void render_game() {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        // Pobierz rozmiary terminala
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        
        // Oblicz rozmiary areny (kwadratowa, ale dopasowana do terminala)
        int arena_size = std::min(max_y - 4, (max_x - 4) / 2) * 2; // Parzysta szerokość dla lepszego wyglądu
        int arena_height = arena_size / 2;
        int arena_width = arena_size;
        
        int start_y = (max_y - arena_height) / 2;
        int start_x = (max_x - arena_width) / 2;
        
        // Wyczyść ekran
        clear();
        
        // Ramka areny z kolorami
        if (has_colors()) attron(COLOR_PAIR(4));
        
        // Górna i dolna ramka
        for (int x = 0; x < arena_width; x++) {
            mvaddch(start_y, start_x + x, '-');
            mvaddch(start_y + arena_height - 1, start_x + x, '-');
        }
        
        // Lewa i prawa ramka
        for (int y = 0; y < arena_height; y++) {
            mvaddch(start_y + y, start_x, '|');
            mvaddch(start_y + y, start_x + arena_width - 1, '|');
        }
        
        // Rogi
        mvaddch(start_y, start_x, '+');
        mvaddch(start_y, start_x + arena_width - 1, '+');
        mvaddch(start_y + arena_height - 1, start_x, '+');
        mvaddch(start_y + arena_height - 1, start_x + arena_width - 1, '+');
        
        if (has_colors()) attroff(COLOR_PAIR(4));
        
        // Narysuj platformy
        for (int i = 0; i < 4; i++) {
            const auto& paddle = game_state.paddles[i];
            
            // Oblicz pozycję platformy
            int paddle_start = (int)((paddle.position - paddle.size/2) * arena_width / ARENA_SIZE);
            int paddle_end = (int)((paddle.position + paddle.size/2) * arena_width / ARENA_SIZE);
            
            paddle_start = std::max(1, std::min(arena_width - 2, paddle_start));
            paddle_end = std::max(1, std::min(arena_width - 2, paddle_end));
            
            // Wybierz kolor i znak
            char paddle_char;
            int color_pair;
            
            if (i == my_player_id) {
                paddle_char = '#';
                color_pair = 1; // Czerwony dla nas
            } else {
                paddle_char = '=';
                color_pair = 2; // Zielony dla innych
            }
            
            if (has_colors()) attron(COLOR_PAIR(color_pair));
            
            switch (paddle.wall) {
                case WALL_NORTH: // Górna ściana
                    for (int x = paddle_start; x <= paddle_end; x++) {
                        mvaddch(start_y + 1, start_x + x, paddle_char);
                    }
                    break;
                    
                case WALL_SOUTH: // Dolna ściana
                    for (int x = paddle_start; x <= paddle_end; x++) {
                        mvaddch(start_y + arena_height - 2, start_x + x, paddle_char);
                    }
                    break;
                    
                case WALL_WEST: // Lewa ściana
                    {
                        int paddle_start_y = (int)((paddle.position - paddle.size/2) * arena_height / ARENA_SIZE);
                        int paddle_end_y = (int)((paddle.position + paddle.size/2) * arena_height / ARENA_SIZE);
                        
                        paddle_start_y = std::max(1, std::min(arena_height - 2, paddle_start_y));
                        paddle_end_y = std::max(1, std::min(arena_height - 2, paddle_end_y));
                        
                        for (int y = paddle_start_y; y <= paddle_end_y; y++) {
                            mvaddch(start_y + y, start_x + 1, paddle_char);
                        }
                    }
                    break;
                    
                case WALL_EAST: // Prawa ściana
                    {
                        int paddle_start_y = (int)((paddle.position - paddle.size/2) * arena_height / ARENA_SIZE);
                        int paddle_end_y = (int)((paddle.position + paddle.size/2) * arena_height / ARENA_SIZE);
                        
                        paddle_start_y = std::max(1, std::min(arena_height - 2, paddle_start_y));
                        paddle_end_y = std::max(1, std::min(arena_height - 2, paddle_end_y));
                        
                        for (int y = paddle_start_y; y <= paddle_end_y; y++) {
                            mvaddch(start_y + y, start_x + arena_width - 2, paddle_char);
                        }
                    }
                    break;
            }
            
            if (has_colors()) attroff(COLOR_PAIR(color_pair));
        }
        
        // Narysuj kulkę
        int ball_x = (int)(game_state.ball.x * arena_width / ARENA_SIZE);
        int ball_y = (int)(game_state.ball.y * arena_height / ARENA_SIZE);
        
        ball_x = std::max(1, std::min(arena_width - 2, ball_x));
        ball_y = std::max(1, std::min(arena_height - 2, ball_y));
        
        if (has_colors()) attron(COLOR_PAIR(3));
        mvaddch(start_y + ball_y, start_x + ball_x, 'O');
        if (has_colors()) attroff(COLOR_PAIR(3));
        
        // Wyświetl tytuł gry
        std::string title = "=== THE 4PONG ===";
        mvprintw(0, (max_x - title.length()) / 2, "%s", title.c_str());
        
        // Wyświetl wyniki z kolorami
        if (has_colors()) attron(COLOR_PAIR(5));
        
        std::string scores_text = "Wyniki: ";
        for (int i = 0; i < 4; i++) {
            scores_text += "Gracz " + std::to_string(i) + ": " + std::to_string(game_state.scores[i]);
            if (i < 3) scores_text += " | ";
        }
        mvprintw(max_y - 3, (max_x - scores_text.length()) / 2, "%s", scores_text.c_str());
        
        // Informacje o sterowaniu
        std::string controls = "Twój ID: " + std::to_string(my_player_id) + " | A/D lub Strzałki | SPACE-stop | Q-wyjście";
        mvprintw(max_y - 2, (max_x - controls.length()) / 2, "%s", controls.c_str());
        
        if (has_colors()) attroff(COLOR_PAIR(5));
        
        // Oznaczenia graczy na rogach
        if (has_colors()) attron(COLOR_PAIR(my_player_id == 0 ? 1 : 2));
        mvprintw(start_y - 1, start_x + arena_width/2 - 3, "Gracz 0");
        if (has_colors()) attroff(COLOR_PAIR(my_player_id == 0 ? 1 : 2));
        
        if (has_colors()) attron(COLOR_PAIR(my_player_id == 1 ? 1 : 2));
        mvprintw(start_y + arena_height/2, start_x + arena_width + 1, "G");
        mvprintw(start_y + arena_height/2 + 1, start_x + arena_width + 1, "1");
        if (has_colors()) attroff(COLOR_PAIR(my_player_id == 1 ? 1 : 2));
        
        if (has_colors()) attron(COLOR_PAIR(my_player_id == 2 ? 1 : 2));
        mvprintw(start_y + arena_height, start_x + arena_width/2 - 3, "Gracz 2");
        if (has_colors()) attroff(COLOR_PAIR(my_player_id == 2 ? 1 : 2));
        
        if (has_colors()) attron(COLOR_PAIR(my_player_id == 3 ? 1 : 2));
        mvprintw(start_y + arena_height/2, start_x - 2, "G");
        mvprintw(start_y + arena_height/2 + 1, start_x - 2, "3");
        if (has_colors()) attroff(COLOR_PAIR(my_player_id == 3 ? 1 : 2));
        
        // Odśwież ekran
        refresh();
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int port = 8080;
    
    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }
    
    GameClient client;
    
    if (!client.connect_to_server(server_ip, port)) {
        std::cerr << "Nie można połączyć z serwerem\n";
        return 1;
    }
    
    std::string nick;
    std::cout << "Podaj nick (max 20 znaków): ";
    std::getline(std::cin, nick);
    
    if (!client.join_lobby(nick)) {
        std::cerr << "Nie można dołączyć do lobby\n";
        return 1;
    }
    
    std::cout << "Naciśnij ENTER aby zaznaczyć gotowość...";
    std::cin.get();
    
    client.set_ready();
    client.wait_for_game();
    
    return 0;
}