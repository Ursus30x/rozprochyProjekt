# The 4Pong - Czterozawodnikowy Pong 🎮

Innowacyjna gra sieciowa dla 4 graczy inspirowana klasycznym Pongiem. Każdy gracz kontroluje platformę na jednej ze ścian kwadratowej areny i stara się odbić kulkę tak, aby inni gracze nie zdołali jej odbić.

## 🚀 Szybki start

### Kompilacja
```bash
make all
```

### Uruchomienie serwera
```bash
make run-server
# lub
./the4pong_server 8080
```

### Uruchomienie klienta
```bash
make run-client  
# lub
./the4pong_client 127.0.0.1 8080
```

## 📁 Struktura plików

### Po stronie serwera:
- `server.cpp` - Główny plik serwera gry
- `common.h` - Wspólne struktury i definicje
- `the4pong_server` - Plik wykonwalny serwera (po kompilacji)

### Po stronie klienta:
- `client.cpp` - Główny plik klienta gry  
- `common.h` - Wspólne struktury i definicje
- `the4pong_client` - Plik wykonwalny klienta (po kompilacji)

### Pliki wspólne:
- `Makefile` - Skrypt kompilacji
- `README.md` - Ta instrukcja

## 🎯 Jak grać

### Przygotowanie gry:
1. Uruchom serwer na wybranym porcie
2. Każdy z 4 graczy uruchamia klienta i łączy się z serwerem
3. Gracze podają swoje nicki i zaznaczają gotowość
4. Gdy wszyscy są gotowi, gra się rozpoczyna automatycznie

### Sterowanie:
- **A** lub **←** - ruch platformy w lewo
- **D** lub **→** - ruch platformy w prawo  
- **SPACE** - zatrzymanie ruchu
- **Q** lub **ESC** - wyjście z gry

### Interfejs ncurses:
- **Kolorowy interfejs** - twoja platforma jest czerwona, inne zielone
- **Kulka żółta** - łatwa do śledzenia
- **Ramka cyjan** - czytelne granice areny
- **Adaptacyjny rozmiar** - dostosowuje się do rozmiaru terminala
- **Informacje na żywo** - wyniki i sterowanie zawsze widoczne

### Rozgrywka:
- Każdy gracz kontroluje platformę na jednej ze ścian areny
- Gracz 0: górna ściana
- Gracz 1: prawa ściana  
- Gracz 2: dolna ściana
- Gracz 3: lewa ściana
- Cel: odbij kulkę tak, aby inni gracze nie zdołali jej odbić
- Gdy kulka dotknie ściany bez odbicia przez platformę, gracz traci punkt
- Gra kończy się gdy zostanie jeden gracz lub wszyscy stracą punkty

## 🔧 Opcje kompilacji

```bash
make help          # Wyświetla dostępne opcje
make server        # Kompiluje tylko serwer
make client        # Kompiluje tylko klienta  
make clean         # Usuwa pliki wykonywalne
make install       # Instaluje do /usr/local/bin
make test          # Uruchamia serwer dla testów
make stop          # Zatrzymuje wszystkie procesy gry
```

## 🌐 Architektura sieciowa

### Protokoły komunikacji:
- **TCP** - Niezawodne komunikaty (dołączanie, gotowość, koniec gry)
- **UDP** - Szybkie akcje gracza i synchronizacja stanu gry

### Synchronizacja:
- Akcje graczy propagowane natychmiast do wszystkich klientów
- Stan gry synchronizowany co 100ms
- Gra działa z częstotliwością 60 FPS

### Bezpieczeństwo:
- Serwer autoryzuje wszystkie ruchy graczy
- Klienci wysyłają tylko akcje, nie pozycje
- Sprawdzanie sum kontrolnych programu

## 🎮 Mechaniki gry

### Kolizje kulki:
- Odbicie od platform zależy od miejsca uderzenia
- Uderzenie w środek = odbicie proste
- Uderzenie przy krawędzi = odbicie pod kątem
- Specjalna obsługa rogów areny

### System punktowy:
- Każdy gracz zaczyna z 5 punktami
- Utrata punktu za niepowodzenie w obronie
- Gra kończy się gdy zostanie ≤1 graczy

## 🔍 Rozwiązywanie problemów

### Błąd połączenia:
```bash
# Sprawdź czy serwer działa
ps aux | grep the4pong_server

# Sprawdź port
netstat -tlnp | grep 8080
```

### Błąd kompilacji ncurses:
```bash
# Zainstaluj bibliotekę ncurses
sudo apt-get install libncurses5-dev

# Sprawdź czy jest dostępna
pkg-config --exists ncurses && echo "OK" || echo "Brak ncurses"

# Alternatywnie użyj make check-deps
make check-deps
```

### Problemy z terminalem:
- Gra wymaga terminala obsługującego kody ANSI
- Zalecane: terminal gnome, xterm, konsole

## 👥 Autorzy

- Jakub Szymczyk 198134
- Krzysztof Taraszkiewicz 197796  
- Józef Sztabiński 197890
- Jakub Drejka 198083

## 📋 Wymagania systemowe

- Linux (testowane na Ubuntu 20.04+)
- g++ z obsługą C++17
- biblioteki: pthread, socket
- **ncurses** (dla klienta): `sudo apt-get install libncurses5-dev`

### Instalacja zależności:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libncurses5-dev

# CentOS/RHEL
sudo yum install gcc-c++ ncurses-devel

# Arch
sudo pacman -S base-devel ncurses
```

---

**Powodzenia w grze! Niech wygra najlepszy gracz! 🏆**# rozprochyProjekt
