#include "mpi.h"
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <map>
#include <stack>
#include <unistd.h>
#include <vector>

using std::map;
using std::stack;
using std::vector;

struct Fork {
    int id;
    bool clean;
    int owner;
    int alt_owner;

    Fork() {}
    Fork(int _id, bool _clean, int _owner, int _alt_owner) :
        id(_id), clean(_clean), owner(_owner), alt_owner(_alt_owner) {}
};

struct ForkMessage{
    int id;
    int sender;
    int type;

    ForkMessage() {}
    ForkMessage(int _id, int _sender, int _type) : id(_id), sender(_sender), type(_type) {}
};

#define FORK_REQUEST 0
#define FORK_RESPONSE 1

inline void _print_spaces(int i){
    while(i--) putchar(' ');
}

#define MSG_PRINT(format, ...) \
do{ \
    printf("<process %d> :: " format "\n", k, ##__VA_ARGS__ ); \
    fflush(stdout); \
} while(0)

#define STAT_PRINT(format, ...) \
do{ \
    _print_spaces(k); \
    printf(format "\n", ##__VA_ARGS__ ); \
    fflush(stdout); \
} while(0)

#define DUMP_VECTOR(v) \
do{ \
    printf("<process %d forks> ::\n", k); \
    for(vector<Fork>::const_iterator it = v.begin(); it != v.end(); ++it) \
        printf("{%d, %c, (%d) <-> %d}, ", it->id, it->clean ? 'T' : 'F', it->owner, it->alt_owner); \
    printf("\n"); \
    fflush(stdout); \
} while(0)

void parse_request(ForkMessage msg, vector<Fork> &forks, stack<ForkMessage> &requests, int k){
    // MSG_PRINT("Parsing a request: {%s, F=%d, S=%d}", msg.type ? "RES" : "REQ", msg.id, msg.sender);
    for(int i = 0; i < 2; ++i)
        if(forks[i].id == msg.id){
            if(forks[i].owner == k){
                if(!forks[i].clean){
                    ForkMessage outbound = ForkMessage(forks[i].id, k, FORK_RESPONSE);
                    // MSG_PRINT("Passing the fork %d to %d.", msg.id, msg.sender);
                    forks[i].owner = msg.sender;
                    forks[i].alt_owner = k;
                    forks[i].clean = true;
                    MPI_Send(&outbound, sizeof(ForkMessage), MPI_BYTE, msg.sender, 0, MPI_COMM_WORLD);
                } else {
                    requests.push(msg);
                }
            } else { // I don't have it yet
                requests.push(msg);
            }
        }
}

void parse_response(ForkMessage msg, vector<Fork> &forks, int k){
    // MSG_PRINT("Parsing a response: {%s, F=%d, S=%d}", msg.type ? "RES" : "REQ", msg.id, msg.sender);
    for(int i = 0; i < 2; ++i)
        if(forks[i].id == msg.id){
            forks[i].clean = true;
            forks[i].owner = k;
            forks[i].alt_owner = msg.sender;
        }
    // MSG_PRINT("Got a fork %d from %d.", msg.id, msg.sender);
}

int main(int argc, char *argv[]){
    int N, k, name_len;
    char processor_name[32];

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &k);
    MPI_Get_processor_name(processor_name, &name_len);

    vector<Fork> my_forks;
    stack<ForkMessage> requests;

    if(k == 0){
        my_forks.push_back(Fork(0, false, 0, 1));
        my_forks.push_back(Fork(1, false, 0, 1));
    } else if(k == N-1){
        my_forks.push_back(Fork(N - 1, false, N - 2, N - 1));
        my_forks.push_back(Fork(0, false, 0, N - 1));
    } else {
        my_forks.push_back(Fork(k, false, k - 1, k));
        my_forks.push_back(Fork(k + 1, false, k, k + 1));
    }

    srand(k + 1);

    // MSG_PRINT("Starting; my fork ids are = (%d, %d)", my_forks[0].id, my_forks[1].id);

    int sleep_time, c_time = 0, sleep_count = 10;

    while(true){
        // DUMP_VECTOR(my_forks);
        sleep_time = rand() % 500 * 10;
        c_time = 0;

        // MSG_PRINT("Going to think!");
        // printf("\n");

        STAT_PRINT("mislim");

        while(c_time < sleep_time){
            usleep(10000);
            c_time += 10;
            int flag;
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
            if(flag){
                // MSG_PRINT("Received a message while sleeping.");
                ForkMessage incoming;
                MPI_Recv(&incoming, sizeof(ForkMessage), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                parse_request(incoming, my_forks, requests, k);
            }
        }

        // MSG_PRINT("Thought for <%dms>, now want to eat!", sleep_time);

        bool has_all = false;
        while(!has_all){
            has_all = true;
            for(int i = 0; i < 2; ++i){
                if(my_forks[i].owner != k){
                    // MSG_PRINT("I don't have a fork %d! -> Asking %d for it.", my_forks[i].id, my_forks[i].owner);
                    STAT_PRINT("trazim vilicu (%d)", my_forks[i].id);
                    ForkMessage outbound = ForkMessage(my_forks[i].id, k, FORK_REQUEST);
                    MPI_Send(&outbound, sizeof(ForkMessage), MPI_BYTE, my_forks[i].owner, 0, MPI_COMM_WORLD);
                    has_all = false;

                    while(my_forks[i].owner != k){
                        ForkMessage incoming;
                        MPI_Recv(&incoming, sizeof(ForkMessage), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        if(incoming.type == FORK_RESPONSE)
                            parse_response(incoming, my_forks, k);
                        else
                            parse_request(incoming, my_forks, requests, k);
                    }
                }
            }

        }

        //MSG_PRINT("I have all the forks, EATING!");
        STAT_PRINT("jedem");

        for(int i = 0; i < 2; ++i)
            my_forks[i].clean = false;

        while(!requests.empty()){
            ForkMessage top = requests.top(); requests.pop();
            ForkMessage outbound = ForkMessage(top.id, k, FORK_REQUEST);
            MPI_Send(&outbound, sizeof(ForkMessage), MPI_BYTE, top.sender, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}