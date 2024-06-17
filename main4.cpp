#include <ncurses.h>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <queue>

using namespace std;

mutex ncurses_mutex;
atomic<bool> is_running(true);

vector<mutex> path_mutex(3);
bool isClear[3] = {true, true, true};

condition_variable path_cv[3];

mutex queue_mutex;
queue<shared_ptr<class Client>> clientQueue; // Kolejka klientów oczekujących

condition_variable queue_cv;

mutex direction_mutex;
int direction = 0;
char symbol = 'F';

class Distributor {
public:
    static const int x = 60;  
    static const int y = 10;

    static void change_direction() {
        int direction_sequence[] = {0, 1, 2};  
        int index = 0;  
        
        while (is_running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));  
            {
                lock_guard<mutex> lock(direction_mutex);
                direction = direction_sequence[index];  
                symbol = (direction == 0 ? 'F' : (direction == 1 ? 'R' : 'L'));
                index = (index + 1) % 3;   
                path_cv[direction].notify_all(); // Notify all clients of direction change
            }
            {
                lock_guard<mutex> lock(queue_mutex);
                queue_cv.notify_all();
            }
            
            
        }
    }
};

class Client : public std::enable_shared_from_this<Client> {
public:
    char symbol;  
    int x;        
    int y = 10;  
    bool afterChange = false;
    int x_after = 0;
    bool right = false;
    bool left = false;
    int sleep_duration;
    unique_lock<mutex> lock;
    int global_dir = 0;
    bool before = true;
    thread threadId;
    volatile bool active = true;

    Client(char sym, int start_x) : symbol(sym), x(start_x) {
        sleep_duration = 50 + rand() % 100;
    }

    void operator()() {
        while (x < COLS && is_running.load() && active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration));
            
            if (x == Distributor::x && before) {
                {
                    unique_lock<mutex> lock(queue_mutex);
                    clientQueue.push(shared_from_this()); // Dodanie do kolejki

                    // Oczekiwanie, aż klient będzie pierwszy w kolejce
                    queue_cv.wait(lock, [&] { 
                        return clientQueue.front().get() == this && isClear[direction]; 
                    });
                }

                {
                    unique_lock<mutex> lock(path_mutex[direction]);

                    global_dir = direction;
                    isClear[global_dir] = false; // Oznaczenie ścieżki jako zajętej
                }

                {
                    unique_lock<mutex> lock(queue_mutex);
                    clientQueue.pop(); // Usunięcie z kolejki
                    queue_cv.notify_all();
                }

                adjust_direction(global_dir);
                before = false;
            }

            if (afterChange && x_after <= 3) {
                if (right) {
                    y += 1;
                } else {
                    y -= 1;
                }
                x_after += 1;
            } else {
                x += 1;
            }

            if (x == 120) {
                {
                    lock_guard<mutex> lock(path_mutex[global_dir]);
                    isClear[global_dir] = true;
                    path_cv[global_dir].notify_all();
                    active = false;
                }
            }
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

vector<shared_ptr<Client>> clients_data;
vector<thread> client_threads;

void refresh_screen() {
    while (is_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lock_guard<mutex> lk(ncurses_mutex);
        clear();
        for (const auto& client : clients_data) {
            if (client->active) {
                mvaddch(client->y, client->x, client->symbol);
            }
        }
        
        mvaddch(Distributor::y, Distributor::x, symbol);  
        mvaddch(10, 120, 'F');
        mvaddch(14, 120, 'R'); 
        mvaddch(6, 120, 'L');
        refresh();
    }
}

void check_for_space_key() {
    while (is_running) {
        int ch = getch();
        if (ch == ' ') {
            is_running = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);


    std::thread distributor(Distributor::change_direction);
    std::thread refresher(refresh_screen);
    std::thread key_checker(check_for_space_key);

    auto lastCreationTime = chrono::steady_clock::now();

    while (is_running) {
        auto currentTime = chrono::steady_clock::now();
        float elapsed = chrono::duration<float>(currentTime - lastCreationTime).count();
        float generationTime = 3;
        if (elapsed >= generationTime) {
            char symbol = 'A' + (symbol_index % 26);
            symbol_index++;

            auto client_data = make_shared<Client>(symbol, 0);

            clients_data.push_back(client_data);
  
            client_data->threadId = thread(&Client::operator(), client_data);
            client_threads.emplace_back(std::move(client_data->threadId));

            lastCreationTime = currentTime;
        }

    }

    distributor.join();
    refresher.join();
    key_checker.join();

    for (auto& client_thread : client_threads) {
        if (client_thread.joinable()) {
            client_thread.join();
        }
    }

    endwin();
    return 0;
}
do make
run4:
	g++ -o main4 main4.cpp -lncurses -pthread
	./main4
