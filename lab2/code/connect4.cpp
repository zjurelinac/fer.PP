#include "mpi.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <map>
#include <time.h>
#include <unistd.h>
#include <vector>

////////////////////////////////////////////////////////////////////////////////

#define MAX_BRANCH_PATH 5
#define MAX_MSG_PAYLOAD 256

#define PLAYER 1
#define COMPUTER 2
#define OTHER(p) (3 - (p))

#define BRANCH_DEPTH 2
#define TASK_DEPTH 6

////////////////////////////////////////////////////////////////////////////////

#define MSG_PRINT(format, ...) \
do{ \
    printf("<process %d> :: " format "\n", k, ##__VA_ARGS__ ); \
    fflush(stdout); \
} while(0)

////////////////////////////////////////////////////////////////////////////////

// Message types
/*  WAKE   M -> W   == broadcast
    WHAT?  W -> M
        SLEEP  M -> W
        EXIT   M -> W
        TASK   M -> W
            SOLUTION   W -> M*/

enum MessageType{
    WAKE, WHAT, SLEEP, EXIT, TASK, SOLUTION
};

struct Message{
    MessageType type;
    char payload[MAX_MSG_PAYLOAD];
    int payload_size;

    Message *set_wake_message(){
        type = WAKE;
        memset(payload, 0, sizeof(payload));
        payload_size = 0;
        return this;
    }

    Message *set_what_message(){
        type = WHAT;
        memset(payload, 0, sizeof(payload));
        payload_size = 0;
        return this;
    }

    Message *set_sleep_message(){
        type = SLEEP;
        memset(payload, 0, sizeof(payload));
        payload_size = 0;
        return this;
    }

    Message *set_exit_message(){
        type = EXIT;
        memset(payload, 0, sizeof(payload));
        payload_size = 0;
        return this;
    }

    Message *set_task_message(void *data, int size){
        type = TASK;
        memcpy(payload, data, size);
        payload_size = size;
        return this;
    }

    Message *set_solution_message(void *data, int size){
        type = SOLUTION;
        memcpy(payload, data, size);
        payload_size = size;
        return this;
    }

    int send(int to){
        return MPI_Send(this, sizeof(Message), MPI_BYTE, to, 0, MPI_COMM_WORLD);
    }

    int receive(int from, MPI_Status &mpi_stat){
        return MPI_Recv(this, sizeof(Message), MPI_BYTE, from, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_stat);
    }

    int broadcast(int root){
        return MPI_Bcast(this, sizeof(Message), MPI_BYTE, root, MPI_COMM_WORLD);
    }
};

////////////////////////////////////////////////////////////////////////////////

struct GameException: public std::exception{
    std::string message;

    GameException(std::string msg) : message(msg) {}

    virtual const char* what() const throw(){
        return message.c_str();
    }

    virtual ~GameException() throw() {}
};

struct Column{
    unsigned int col;

    Column(){ col = 0; }
    void set(int pos, int player);
    int get(int pos);
    int max_pos();
};

struct Board{
    static const int R = 6, C = 7;
    Column cols[7];

    Board(){}
    void set(int xpos, int ypos, int player);
    int get(int xpos, int ypos);
    bool check_win(int xpos, int ypos);
    bool place(int xpos, int player);
    void draw();

    static bool valid_pos(int xpos, int ypos);
};


void Column::set(int pos, int player){
    col = (col & ~(3 << 2*pos)) | (player << 2*pos);
}

int Column::get(int pos){
    return (col >> 2*pos) & 3;
}

int Column::max_pos(){
    for(int i = 5; i >= 0; --i)
        if(get(i) > 0)
            return i;
    return -1;
}


void Board::set(int xpos, int ypos, int player){
    if(!valid_pos(xpos, ypos))
        throw GameException("Invalid position!");
    cols[xpos].set(ypos, player);
}

int Board::get(int xpos, int ypos){
    return cols[xpos].get(ypos);
}

bool Board::place(int xpos, int player){
    int ypos = cols[xpos].max_pos() + 1;
    set(xpos, ypos, player);
    return check_win(xpos, ypos);
}

bool Board::check_win(int xpos, int ypos){
    const int dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1},
              dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1},
              pairs[4][2] = {{0, 7}, {1, 6}, {2, 5}, {3, 4}};

    int init = get(xpos, ypos), cnt[8] = {0};

    if(init == 0)
        return false;

    for(int k = 0; k < 8; ++k){
        int x = xpos, y = ypos;
        while(valid_pos(x, y) && get(x, y) == init){
            x += dx[k];
            y += dy[k];
            ++cnt[k];
        }
    }

    for(int i = 0; i < 4; ++i)
        if(cnt[pairs[i][0]] + cnt[pairs[i][1]] >= 5)
            return true;
    return false;
}

void Board::draw(){
    printf("  +-------+\n");
    for(int j = 5; j >= 0; --j){
        printf("%d |", j);
        for(int i = 0; i < 7; ++i){
            int v = get(i, j);
            printf("%c", v == 0 ? '.' : (v == 1 ? '1' : '2'));
        }
        printf("|\n");
    }
    printf("  +-------+\n   0123456\n\n");
}

bool Board::valid_pos(int xpos, int ypos){
    return (xpos >= 0 && xpos < C && ypos >= 0 && ypos < R);
}

////////////////////////////////////////////////////////////////////////////////

//typedef std::vector<int> PositionKey;

struct PositionKey{
    int pos[MAX_BRANCH_PATH];
    int len;

    PositionKey() : len(0) {}

    const bool operator < (const PositionKey &pk) const{
        if(len == pk.len){
            for(int i = 0; i < len; ++i)
                if(pos[i] != pk.pos[i])
                    return pos[i] < pk.pos[i];
            return false;
        } else
            return len < pk.len;
    }

    bool operator == (const PositionKey &pk){
        if(len == pk.len){
            for(int i = 0; i < len; ++i)
                if(pos[i] != pk.pos[i])
                    return false;
            return true;
        } else
            return false;
    }

    PositionKey& operator = (const PositionKey &pk){
        len = pk.len;
        memcpy(pos, pk.pos, sizeof(pos));
        return *this;
    }

    int operator[] (int i){
        return pos[i];
    }

    void push_back(int x){
        pos[len++] = x;
    }
};

struct Task{
    Board b;
    int next_player;
    PositionKey pk;

    Task(){}
    Task(Board b, int next_player, PositionKey pk)
        : b(b), next_player(next_player), pk(pk) {}

    void show_pk(){
        printf("[");
        for(int i = 0; i < pk.len; ++i)
            printf("%d, ", pk[i]);
        printf("]\n");
    }
};

struct Solution{
    PositionKey pk;
    float value;

    Solution(){}
    Solution(PositionKey pk, float value) : pk(pk), value(value) {}
};

////////////////////////////////////////////////////////////////////////////////

int ask_move(){
    int move;
    printf("Player move (0-6):> ");
    scanf("%d", &move);
    puts("=====================");
    return move;
}

void generate_tasks(Board b, PositionKey tpos, int current_player, int current_move,
                    int depth, std::deque<Task> &dq){
    if(current_move != -1){ // for initial call
        if(b.place(current_move, current_player)) // game over here
            return;
        tpos.push_back(current_move);
    }

    if(depth >= BRANCH_DEPTH)
        dq.push_back(Task(b, OTHER(current_player), tpos));
    else {
        for(int move = 0; move < 7; ++move)
            if(b.cols[move].max_pos() < b.R - 1) // possible move
                generate_tasks(b, tpos, OTHER(current_player), move, depth + 1, dq);
    }
}

float calculate_state_value(Board b, int current_player, int current_move, int depth){
    if(current_move != -1)
        if(b.place(current_move, current_player)) // if this is a winning move
            return (current_player == COMPUTER ? 1 : -1);

    if(depth >= TASK_DEPTH)
        return 0;
    else {
        int move_cnt = 0;
        float sum = 0, ivalue;
        for(int move = 0; move < 7; ++move)
            if(b.cols[move].max_pos() < b.R - 1){ // possible move
                ivalue = calculate_state_value(b, OTHER(current_player), move, depth + 1);

                if(ivalue == -1 && current_player == PLAYER)
                    return -1;

                if(ivalue == 1 && current_player == COMPUTER)
                    return 1;

                sum += ivalue;
                ++move_cnt;
            }
        return sum / move_cnt;
    }
}

float calculate_move_value(Board b, PositionKey tpos, int current_player, int current_move,
                           int depth, std::map<PositionKey, float> &task_results){
    if(b.place(current_move, current_player))  // if this is a winning move
        return (current_player == COMPUTER ? 1 : -1);

    tpos.push_back(current_move);

    if(depth >= BRANCH_DEPTH)
        return task_results[tpos];
    else {
        int move_cnt = 0;
        float sum = 0, ivalue;
        for(int move = 0; move < 7; ++move)
            if(b.cols[move].max_pos() < b.R - 1){ // possible move
                ivalue = calculate_move_value(b, tpos, OTHER(current_player), move, depth + 1, task_results);

                if(ivalue == -1 && current_player == PLAYER)
                    return -1;

                if(ivalue == 1 && current_player == COMPUTER)
                    return 1;

                sum += ivalue;
                ++move_cnt;
            }
        return sum / move_cnt;
    }
}

int calculate_computer_move(Board b, int N){
    Message msg;
    MPI_Status mpi_stat;
    int k = 0, stopped_workers = 0;
    Task task;
    Solution solution;
    std::deque<Task> task_queue;
    std::map<PositionKey, float> task_results;
    clock_t starttime, endtime;

    starttime = clock();

    // generate tasks
    generate_tasks(b, PositionKey(), PLAYER, -1, 0, task_queue);

    if(N > 1){
        // wake workers
        msg.set_wake_message()->broadcast(0);

        // process all tasks
        while(!task_queue.empty() > 0 || stopped_workers < N - 1){

            msg.receive(MPI_ANY_SOURCE, mpi_stat);

            if(msg.type == SOLUTION){
                memcpy(&solution, msg.payload, msg.payload_size);
                task_results[solution.pk] = solution.value;
                //MSG_PRINT("Received a SOLUTION from %d :: %.5f", mpi_stat.MPI_SOURCE, solution.value);
            } else if(msg.type == WHAT){
                //MSG_PRINT("Received a WHAT? from %d", mpi_stat.MPI_SOURCE);
            }

            if(!task_queue.empty()){
                Task task = task_queue.front(); task_queue.pop_front();
                msg.set_task_message(&task, sizeof(task))->send(mpi_stat.MPI_SOURCE);
            } else {
                ++stopped_workers;
                msg.set_sleep_message()->send(mpi_stat.MPI_SOURCE);
            }
        }
    } else {  // special case
        for(auto task : task_queue)
            task_results[task.pk] = calculate_state_value(task.b, OTHER(task.next_player), -1, 0);
    }

    // collect task results and calculate best solution
    float best_sol = -2, curr_sol;
    int best_move = -1;
    for(int move = 0; move < 7; ++move){
        if(b.cols[move].max_pos() < b.R - 1){  // if possible move
            curr_sol = calculate_move_value(b, PositionKey(), COMPUTER, move, 1, task_results);
            if(curr_sol > best_sol){
                best_sol = curr_sol;
                best_move = move;
            }
        }
    }
    printf("Best computer move %d with score %.5f\n", best_move, best_sol);

    endtime = clock();
    printf("Elapsed CPU time: %.2f\n", (endtime - starttime) / (float) CLOCKS_PER_SEC);

    return best_move;
}

////////////////////////////////////////////////////////////////////////////////

void master(int N){
    int move, winner = 0;
    bool over;
    Board b;

    sleep(1);

    puts("=============\n  New game  \n=============");
    while(winner == 0){
        b.draw();

        // PLAYER's move
        while(true){
            try{
                move = ask_move();
                over = b.place(move, PLAYER);

                if(over)
                    winner = PLAYER;

                break;
            } catch(GameException ge){
                printf("> %s\n=====================\n", ge.what());
            }
        }

        if(winner != 0)
            break;

        // COMPUTER's move
        move = calculate_computer_move(b, N);
        printf("Computer move (0-6):> %d\n", move);
        over = b.place(move, COMPUTER);

        if(over)
            winner = COMPUTER;

        sleep(1);
    }

    printf("\n=====================\n%d wins!!!\n\n", winner);
    b.draw();

    // stop all workers
    Message().set_exit_message()->broadcast(0);
    puts("Game engine terminated.");
}

void worker(int k){
    Message msg;
    MPI_Status mpi_stat;
    Task task;
    Solution solution;

    while(true){ // until the game is done

        msg.broadcast(0);  // wait for master command
        MSG_PRINT("Received new broadcast :: <%d, %d>", msg.type, msg.payload_size);

        if(msg.type == EXIT){
            MSG_PRINT("Received an EXIT.");
            break;
        }

        msg.set_what_message()->send(0);    // initial WHAT? message

        while(true){  // while there are tasks
            msg.receive(0, mpi_stat);
            if(msg.type == SLEEP){
                //MSG_PRINT("Received a SLEEP.");
                break;
            } else if(msg.type == TASK){
                memcpy(&task, msg.payload, msg.payload_size);
                //MSG_PRINT("Received a TASK");

                solution.pk = task.pk;
                solution.value = calculate_state_value(task.b, OTHER(task.next_player), -1, 0);

                msg.set_solution_message(&solution, sizeof(solution))->send(0);
            } else {
                MSG_PRINT("Unknown message type :: %d", msg.type);
            }
        }
    }

}

void test_master(int N){
    int k = 0, steps = 5, task, remaining_tasks, stopped_workers;
    Message msg;
    MPI_Status mpi_stat;

    for(int step = 0; step < steps; ++step){
        printf("\n============================\nStarting next iteration :: %d\n============================\n", step);

        msg.set_wake_message()->broadcast(0);

        remaining_tasks = 7;
        stopped_workers = 0;

        while(remaining_tasks > 0 || stopped_workers < N - 1){
            msg.receive(MPI_ANY_SOURCE, mpi_stat);

            if(msg.type == SOLUTION){
                int sol = *((int*) msg.payload);
                MSG_PRINT("Received a SOLUTION from %d :: %d", mpi_stat.MPI_SOURCE, sol);
            } else if(msg.type == WHAT){
                MSG_PRINT("Received a WHAT? from %d", mpi_stat.MPI_SOURCE);
            }

            if(remaining_tasks > 0){
                task = 8 - remaining_tasks;
                --remaining_tasks;
                MSG_PRINT(">> Tasks remaining = %d", remaining_tasks);
                msg.set_task_message(&task, sizeof(int))->send(mpi_stat.MPI_SOURCE);
            } else {
                ++stopped_workers;
                MSG_PRINT(">> Sending a sleep to %d.", mpi_stat.MPI_SOURCE);
                msg.set_sleep_message()->send(mpi_stat.MPI_SOURCE);
            }
        }
    }

    MSG_PRINT("Done with the job.");
    msg.set_exit_message()->broadcast(0);
}

void test_worker(int k){
    Message msg;
    MPI_Status mpi_stat;
    int task, solution;

    while(true){  // iteration-level loop

        msg.broadcast(0);
        MSG_PRINT("Received new broadcast :: <%d, %d>", msg.type, msg.payload_size);

        if(msg.type == EXIT){
            MSG_PRINT("Received an EXIT.");
            break;
        }

        msg.set_what_message()->send(0);

        while(true){  // task-level loop
            msg.receive(0, mpi_stat);
            if(msg.type == SLEEP){
                MSG_PRINT("Received a SLEEP.");
                break;
            } else if(msg.type == TASK){
                task = *((int*) msg.payload);
                MSG_PRINT("Received a TASK :: %d", task);

                solution = task * task + 1;
                sleep(1);
                msg.set_solution_message(&solution, sizeof(int))->send(0);
            } else {
                MSG_PRINT("Unknown message type :: %d", msg.type);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]){
    int N = 1, k = 0, name_len;
    char processor_name[32] = "asus";

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &k);
    MPI_Get_processor_name(processor_name, &name_len);

    MSG_PRINT("Started at %s", processor_name);

    if(k == 0)
        master(N);
    else
        worker(k);

    MPI_Finalize();
    return 0;
}