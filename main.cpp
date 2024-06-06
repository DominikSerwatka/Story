#include <ncurses.h>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>

std::mutex ncurses_mutex;
std::atomic<bool> is_running(true);

class Distributor {
public:
    static int direction; 
    static const int x = 60;  
    static const int y = 10;
    static char symbol;


    static void change_direction() {
        int direction_sequence[] = {0, 1, 2};  
        int index = 0;  
        
        while (is_running) {
            std::this_thread::sleep_for(std::chrono::seconds(4));  
            direction = direction_sequence[index];  
            symbol = (direction == 0 ? 'F' : (direction == 1 ? 'R' : 'L'));
            
            index = (index + 1) % 3;  
        }
    }
};

int Distributor::direction = 0;
char Distributor::symbol = 'F';

class Client {
public:
    char symbol;  
    int x;        
    int y = 10;  
    bool afterChange = false;
    int x_after = 0;
    bool right = false;
    bool left = false;
    int sleep_duration;

    Client(char sym, int start_x) : symbol(sym), x(start_x) {
        sleep_duration = 50 + rand() % 100;
    }

    void operator()() {
        while (x < COLS && is_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration));
            std::lock_guard<std::mutex> lock(ncurses_mutex);
            mvaddch(y, x, ' ');
            
            if (x == Distributor::x) {
                int local_dir = Distributor::direction;
                adjust_direction(local_dir);
            }

            if (afterChange && x_after <=3) {
              if (right) {
                y += 1;
              } else {
                y -= 1;
              }
              x_after += 1;
              mvaddch(y, x, symbol);
            } else {
              mvaddch(y, ++x, symbol);
            }
            
            
            mvaddch(Distributor::y, Distributor::x, Distributor::symbol);  
            mvaddch( 10, 120, 'F');
            mvaddch( 14, 120, 'R');
            mvaddch( 6, 120, 'L');
            if (x == 120) {
              break;
            }

            refresh();
        }
    }

    void adjust_direction(int dir) {
        
        if (dir == 1) { 
            afterChange = true;
            right = true;
        } else if (dir == 2) {  
            afterChange = true;
            left = true;
        }
        
    }
};

static int symbol_index = 0;

void spawn_clients() {
    std::vector<std::thread> clients;
    while (is_running) { 
        char symbol = 'A' + (symbol_index % 26);
        symbol_index++;
        clients.emplace_back(Client(symbol++, 0));
        std::this_thread::sleep_for(std::chrono::seconds(2 + rand() % 3));
    }

    for (auto& client : clients) {
        if (client.joinable()) {
            client.join();
        }
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    std::thread distributor(Distributor::change_direction);
    std::thread spawner(spawn_clients);

    while ((getch() != ' ')) {}

    is_running = false;
    distributor.join();
    spawner.join();

    endwin();
    return 0;
}
