#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <thread>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <functional>
#include <numeric>
#include <future>
#include <atomic>

using namespace std;
#pragma warning(disable:4996)

struct ListNode {
    function<void()> process;
    ListNode* next;
    bool isBackground; // 백그라운드 프로세스인지 여부를 나타내는 플래그
    ListNode(function<void()>&& proc, bool bg) : process(move(proc)), next(nullptr), isBackground(bg) {}
};

// 스택 노드 구조체 정의
struct StackNode {
    StackNode* next;
    ListNode* list;

    StackNode() : next(nullptr), list(nullptr) {}
};

char** parse(const string& command);
void exec(char** args, const string& original_command);
void echo(const vector<string>& args, int period, int timeout);
void dummy(int count);
int gcd(int a, int b);
void prime(int x);
void sum(int x, int m);
void enqueue(function<void()>&& process, bool isForeground, const string& original_command);
void dequeue();
void promote();
void split_n_merge(StackNode* stack_node);

StackNode* stack = new StackNode();
StackNode* stack_top = stack;
StackNode* P = stack;
mutex stack_mutex;  // 스택 접근을 위한 뮤텍스
mutex print_mutex;  // 출력 보호를 위한 뮤텍스

const int THRESHOLD = 5;  // 임계치 (예시)
atomic<int> background_process_count(0);  // 백그라운드 프로세스 개수를 저장하는 변수

void background_worker() {
    while (true) {
        this_thread::sleep_for(chrono::seconds(1));
        if (background_process_count > 0) {
            dequeue();
        }
    }
}

int main() {
    // 백그라운드 작업자 스레드를 시작
    thread bg_worker(background_worker);
    bg_worker.detach();

    ifstream file("command.txt");
    if (!file.is_open()) {
        cerr << "Failed to open file" << endl;
        return 1;
    }

    string line;
    while (getline(file, line)) {
        try {
            stringstream ss(line);
            string segment;
            while (getline(ss, segment, ';')) {
                segment = segment.substr(0, segment.find_last_not_of(" \t\n\r\f\v") + 1); // Trim trailing whitespace
                char** parsed_command = parse(segment);
                exec(parsed_command, segment);
                for (int i = 0; parsed_command[i][0] != '\0'; i++) {
                    delete[] parsed_command[i];
                }
                delete[] parsed_command;
            }
            promote();  // 프로세스 후 promote 수행
            split_n_merge(stack);  // 프로세스 후 split and merge 수행
        }
        catch (const exception& e) {
            lock_guard<mutex> lock(print_mutex);
            cerr << "Exception occurred: " << e.what() << endl;
        }
    }
    file.close();

    // 프로세스들을 실행
    while (stack_top != nullptr && stack_top->list != nullptr) {
        dequeue();
    }

    return 0;
}

char** parse(const string& command) {
    istringstream stream(command);
    vector<string> tokens;
    string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    char** tokens_array = new char* [tokens.size() + 1];
    for (size_t i = 0; i < tokens.size(); ++i) {
        tokens_array[i] = new char[tokens[i].size() + 1];
        strcpy(tokens_array[i], tokens[i].c_str());
    }
    tokens_array[tokens.size()] = new char[1];
    tokens_array[tokens.size()][0] = '\0';
    return tokens_array;
}

void exec(char** args, const string& original_command) {
    if (args[0][0] == '\0') return;
    string command = args[0];
    bool isBackground = false;
    if (command[0] == '&') {
        isBackground = true;
        command = command.substr(1);
    }

    vector<string> arguments;
    for (int i = 1; args[i][0] != '\0'; i++) {
        arguments.push_back(args[i]);
    }

    int repeat = 1;
    int timeout = 100;
    int period = 1;
    int m = 1;

    // handle_options 함수 내용
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i] == "-n" && i + 1 < arguments.size()) {
            repeat = stoi(arguments[i + 1]);
            arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            --i;
        }
        else if (arguments[i] == "-d" && i + 1 < arguments.size()) {
            timeout = stoi(arguments[i + 1]);
            arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            --i;
        }
        else if (arguments[i] == "-p" && i + 1 < arguments.size()) {
            period = stoi(arguments[i + 1]);
            arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            --i;
        }
        else if (arguments[i] == "-m" && i + 1 < arguments.size()) {
            m = stoi(arguments[i + 1]);
            arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            --i;
        }
    }

    for (int r = 0; r < repeat; ++r) {
        function<void()> process = [=]() {
            {
                lock_guard<mutex> lock(print_mutex);
                cout << "prompt> " << original_command << endl;
                if (isBackground) {
                    cout << "Running: [" << background_process_count.load() << "B]" << endl;
                }
            }
            try {
                if (command == "echo") {
                    echo(arguments, period, timeout);
                }
                else if (command == "dummy") {
                    dummy(1);
                }
                else if (command == "gcd") {
                    if (arguments.size() == 2) {
                        int a = stoi(arguments[0]);
                        int b = stoi(arguments[1]);
                        {
                            lock_guard<mutex> lock(print_mutex);
                            cout << gcd(a, b) << endl;
                        }
                    }
                }
                else if (command == "prime") {
                    if (arguments.size() == 1) {
                        int x = stoi(arguments[0]);
                        prime(x);
                    }
                }
                else if (command == "sum") {
                    if (arguments.size() >= 1) {
                        int x = stoi(arguments[0]);
                        sum(x, m);
                    }
                }
                else {
                    lock_guard<mutex> lock(print_mutex);
                    cerr << "Unknown command: " << command << endl;
                }
            }
            catch (const exception& e) {
                lock_guard<mutex> lock(print_mutex);
                cerr << "Exception occurred in process: " << e.what() << endl;
            }
            this_thread::sleep_for(chrono::seconds(period));
            };

        enqueue(move(process), !isBackground, original_command);
    }
}

void enqueue(function<void()>&& process, bool isForeground, const string& original_command) {
    ListNode* new_node = new ListNode(move(process), !isForeground);
    lock_guard<mutex> lock(stack_mutex);

    if (isForeground) {
        // 최상위 리스트의 끝에 삽입
        ListNode* temp = stack_top->list;
        if (temp == nullptr) {
            stack_top->list = new_node;
        }
        else {
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = new_node;
        }
    }
    else {
        // 최하위 리스트의 끝에 삽입
        StackNode* temp_stack = stack;
        while (temp_stack->next != nullptr) {
            temp_stack = temp_stack->next;
        }

        ListNode* temp = temp_stack->list;
        if (temp == nullptr) {
            temp_stack->list = new_node;
        }
        else {
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = new_node;
        }
        ++background_process_count; // 백그라운드 프로세스 카운트 증가
    }
}

void dequeue() {
    ListNode* node_to_run = nullptr;
    bool isBackgroundProcess = false;
    {
        lock_guard<mutex> lock(stack_mutex);

        if (stack_top == nullptr || stack_top->list == nullptr) {
            cout << "No process to dequeue." << endl;
            return;
        }

        node_to_run = stack_top->list;
        stack_top->list = stack_top->list->next;

        if (stack_top->list == nullptr && stack_top->next != nullptr) {
            StackNode* temp = stack_top;
            stack_top = stack_top->next;
            delete temp;
        }

        isBackgroundProcess = node_to_run->isBackground;
    }

    try {
        // 노드를 실행
        node_to_run->process();
    }
    catch (const exception& e) {
        lock_guard<mutex> lock(print_mutex);
        cerr << "Exception occurred during dequeue: " << e.what() << endl;
    }

    delete node_to_run;

    if (isBackgroundProcess) {
        --background_process_count; // 백그라운드 프로세스 카운트 감소
    }
}

void promote() {
    lock_guard<mutex> lock(stack_mutex);

    if (P == nullptr) return;

    // P가 가리키는 리스트의 헤드 노드를 상위 리스트의 끝에 붙임
    while (P != nullptr && P->list == nullptr) {
        P = P->next; // P가 가리키는 리스트가 비어있으면 다음 스택 노드를 가리키게 함
    }

    if (P == nullptr) {
        P = stack; // 처음으로 되돌림
        return;
    }

    ListNode* node_to_promote = P->list;
    P->list = P->list->next;
    node_to_promote->next = nullptr; // Promote된 노드는 새로운 리스트의 끝이 되므로 next를 null로 설정

    if (stack_top->list == nullptr) {
        stack_top->list = node_to_promote;
    }
    else {
        ListNode* temp = stack_top->list;
        while (temp->next != nullptr) {
            temp = temp->next;
        }
        temp->next = node_to_promote;
    }

    if (P->list == nullptr) {
        StackNode* temp = P;
        P = P->next;
        delete temp;
    }

    if (P == nullptr) {
        P = stack; // 처음으로 되돌림
    }
}

void split_n_merge(StackNode* stack_node) {
    lock_guard<mutex> lock(stack_mutex);

    while (stack_node != nullptr) {
        int list_length = 0;
        ListNode* temp = stack_node->list;

        // 리스트 길이 계산
        while (temp != nullptr) {
            list_length++;
            temp = temp->next;
        }

        if (list_length > THRESHOLD) {
            int split_point = list_length / 2;
            ListNode* split_node = stack_node->list;
            for (int i = 1; i < split_point; ++i) {
                split_node = split_node->next;
            }

            ListNode* new_list = split_node->next;
            split_node->next = nullptr;

            if (stack_node->next == nullptr) {
                stack_node->next = new StackNode();
            }

            ListNode* temp_merge = stack_node->next->list;
            if (temp_merge == nullptr) {
                stack_node->next->list = new_list;
            }
            else {
                while (temp_merge->next != nullptr) {
                    temp_merge = temp_merge->next;
                }
                temp_merge->next = new_list;
            }
        }

        stack_node = stack_node->next;
    }
}

void echo(const vector<string>& args, int period, int timeout) {
    auto start_time = chrono::steady_clock::now();
    while (true) {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - start_time).count();
        if (elapsed >= timeout) {
            break;
        }
        {
            lock_guard<mutex> lock(print_mutex);
            for (const auto& arg : args) {
                cout << arg << " ";
            }
            cout << endl;
        }
        this_thread::sleep_for(chrono::seconds(period));
    }
}

void dummy(int count) {
    for (int i = 0; i < count; ++i) {
        lock_guard<mutex> lock(print_mutex);
    }
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

void prime(int x) {
    vector<bool> is_prime(x + 1, true);
    is_prime[0] = is_prime[1] = false;
    for (int i = 2; i * i <= x; ++i) {
        if (is_prime[i]) {
            for (int j = i * i; j <= x; j += i) {
                is_prime[j] = false;
            }
        }
    }
    int prime_count = count(is_prime.begin(), is_prime.end(), true);
    lock_guard<mutex> lock(print_mutex);
    cout << prime_count << endl;
}

void sum(int x, int m) {
    auto sum_part = [](int start, int end) {
        vector<int> values(end - start);
        iota(values.begin(), values.end(), start);
        return accumulate(values.begin(), values.end(), 0LL);
        };

    int part_size = x / m;
    vector<future<long long>> futures;
    for (int i = 0; i < m; ++i) {
        int start = i * part_size + 1;
        int end = (i == m - 1) ? x + 1 : start + part_size;
        futures.push_back(async(launch::async, sum_part, start, end));
    }

    long long total_sum = 0;
    for (auto& future : futures) {
        total_sum += future.get();
    }
    lock_guard<mutex> lock(print_mutex);
    cout << total_sum / 1000000 << endl;
}
