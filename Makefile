CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread -lncurses

# Pliki źródłowe
SERVER_SRC = server.cpp
CLIENT_SRC = client.cpp
COMMON_HEADER = common.h

# Pliki wykonywalne
SERVER_TARGET = the4pong_server
CLIENT_TARGET = the4pong_client

# Zależności
SERVER_DEPS = 
CLIENT_DEPS = ncurses


# Cele główne
all: check-deps $(SERVER_TARGET) $(CLIENT_TARGET)

# Kompilacja serwera
$(SERVER_TARGET): $(SERVER_SRC) $(COMMON_HEADER)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_SRC) $(LDFLAGS)

# Kompilacja klienta
$(CLIENT_TARGET): $(CLIENT_SRC) $(COMMON_HEADER)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SRC) $(LDFLAGS)

# Tylko serwer
server: $(SERVER_TARGET)

# Tylko klient
client: $(CLIENT_TARGET)

# Uruchomienie serwera na porcie 8080
run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET) 8080

# Uruchomienie klienta (localhost:8080)
run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET) 127.0.0.1 8080

# Czyszczenie
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

# Instalacja (kopiowanie do /usr/local/bin)
install: all
	sudo cp $(SERVER_TARGET) /usr/local/bin/
	sudo cp $(CLIENT_TARGET) /usr/local/bin/

# Odinstalowanie
uninstall:
	sudo rm -f /usr/local/bin/$(SERVER_TARGET)
	sudo rm -f /usr/local/bin/$(CLIENT_TARGET)

# Test - uruchom serwer w tle i 2 klientów
test: all
	@echo "Uruchamianie serwera w tle..."
	./$(SERVER_TARGET) 8080 &
	@echo "Czekanie 2 sekundy..."
	sleep 2
	@echo "Uruchom klientów w osobnych terminalach:"
	@echo "Terminal 1: ./$(CLIENT_TARGET) 127.0.0.1 8080"
	@echo "Terminal 2: ./$(CLIENT_TARGET) 127.0.0.1 8080"
	@echo "Terminal 3: ./$(CLIENT_TARGET) 127.0.0.1 8080"
	@echo "Terminal 4: ./$(CLIENT_TARGET) 127.0.0.1 8080"

# Zatrzymanie wszystkich procesów gry
stop:
	pkill -f $(SERVER_TARGET) || true
	pkill -f $(CLIENT_TARGET) || true

# Pomoc
help:
	@echo "The 4Pong - Czterozawodnikowy Pong"
	@echo ""
	@echo "Dostępne cele:"
	@echo "  all           - Kompiluje serwer i klienta (sprawdza zależności)"
	@echo "  check-deps    - Sprawdza czy wszystkie biblioteki są zainstalowane"
	@echo "  server        - Kompiluje tylko serwer"
	@echo "  client        - Kompiluje tylko klienta"
	@echo "  run-server    - Uruchamia serwer na porcie 8080"
	@echo "  run-client    - Uruchamia klienta (localhost:8080)"
	@echo "  test          - Uruchamia serwer w tle dla testów"
	@echo "  stop          - Zatrzymuje wszystkie procesy gry"
	@echo "  clean         - Usuwa pliki wykonywalne"
	@echo "  install       - Instaluje do /usr/local/bin"
	@echo "  uninstall     - Usuwa z /usr/local/bin"
	@echo "  help          - Wyświetla tę pomoc"
	@echo ""
	@echo "Wymagania:"
	@echo "  - Serwer: g++, pthread"
	@echo "  - Klient: g++, pthread, ncurses"
	@echo "  Instalacja ncurses: sudo apt-get install libncurses5-dev"
	@echo ""
	@echo "Użycie:"
	@echo "  Serwer: ./$(SERVER_TARGET) [port]"
	@echo "  Klient: ./$(CLIENT_TARGET) [ip] [port]"

.PHONY: all server client run-server run-client clean install uninstall test stop help check-deps