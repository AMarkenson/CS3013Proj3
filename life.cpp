#include <iostream>
using namespace std;
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <semaphore.h>
#include <pthread.h>
#include <cstring>



#define MAXTHREAD 10
#define MAXGRID 40
#define RANGE 1
#define ALLDONE 2
#define GO 3
#define GENDONE 4
#define ALLDEAD 5
#define NOCHANGE 6
#define STOP 7

int** genEven = nullptr;
int** genOdd = nullptr;
int gen;

bool print = false;

struct msg {
    int iSender; /* ID of the sender thread (0..number of threads) */
    int type; /* type of message */
    int value1; /* first value */
    int value2; /* second value */
};

msg MAILBOXES[MAXTHREAD + 1]; /* mailboxes for threads 0..MAXTHREAD */
sem_t semSend[MAXTHREAD + 1];
sem_t semRecv[MAXTHREAD + 1];

void printGen(int** grid, int rows, int cols, int generation) {
    cout << "Generation " << generation << ":\n";
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            cout << grid[i][j] << ' ';
        }
        cout << endl;
    }
}

void *sendMsg(int iReceiver, msg* pMsg) {
    // Implementation of sendMsg
    sem_wait(&semSend[iReceiver]);
    MAILBOXES[iReceiver] = *pMsg;
    sem_post(&semRecv[iReceiver]);
    return nullptr;
}

void *recvMsg(int iReceiver, msg* pMsg) {
    // Implementation of recvMsg
    sem_wait(&semRecv[iReceiver]);
    *pMsg = MAILBOXES[iReceiver];
    MAILBOXES[iReceiver] = {0, 0, 0, 0}; // Clear mailbox after receiving
    sem_post(&semSend[iReceiver]);
    return nullptr;
}

void *life(void *arg) {
    int iMe = *((int *) arg);
    msg myMsg, replyMsg;
    int startRow, endRow, rows, cols;
    bool allDead = true, noChange = true;

    //cout << "Thread " << iMe << " started.\n";

    recvMsg(iMe, &replyMsg);
    if (replyMsg.type != RANGE) {
        cerr << "Thread " << iMe << " expected RANGE message but received different type." << endl;
        pthread_exit(nullptr);
    }
    startRow = replyMsg.value1;
    endRow = replyMsg.value2;
    //cout << "Thread " << iMe << " received RANGE: rows " << replyMsg.value1 << " to " << replyMsg.value2 << endl;
    for (int g = 1; g <= gen; g++) {
        recvMsg(iMe, &replyMsg);
        if (replyMsg.type == STOP) {
            //cout << "Thread " << iMe << " received STOP message. Exiting.\n";
            pthread_exit(nullptr);
        }
        if (replyMsg.type == GO) {
            // Get dimensions of the grid
            if(0 == rows) rows = replyMsg.value1; // Number of rows
            if(0 == cols) cols = replyMsg.value2; // Number of columns
            //cout << rows << " rows, " << cols << " columns\n";
            // Determine which generation array to read from and which to write to
            int** currentGen = (g % 2 == 1) ? genEven : genOdd;
            int** nextGen = (g % 2 == 1) ? genOdd : genEven;
            // Update the cells in the assigned rows and correct generation array
            //cout << "Thread " << iMe << " processing rows " << startRow << " to " << endRow << " for generation " << g << endl;
            for (int i = startRow; i < endRow; i++) {
                for (int j = 0; j < cols; j++) {
                    int liveNeighbors = 0;
                    // Count live neighbors
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            if (di == 0 && dj == 0) continue; // Skip current cell
                            int ni = i + di;
                            int nj = j + dj;
                            if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                                liveNeighbors += currentGen[ni][nj];
                            }
                        }
                    }
                    //cout << "Thread " << iMe << " at cell (" << i << "," << j << ") with " << liveNeighbors << " live neighbors." << endl;
                    // Apply Game of Life rules
                    if (currentGen[i][j] == 1) { // Cell is currently alive
                        nextGen[i][j] = (liveNeighbors == 2 || liveNeighbors == 3) ? 1 : 0;
                    } else { // Cell is currently dead
                        nextGen[i][j] = (liveNeighbors == 3) ? 1 : 0;
                    }
                }
            }
        } else {
            cerr << "Thread " << iMe << " expected GO message but received different type." << endl;
            pthread_exit(nullptr);
        }

        // Check for allDead and noChange conditions
        int** nextGen = (g % 2 == 1) ? genOdd : genEven;
        int** currentGen = (g % 2 == 1) ? genEven : genOdd;
        for (int i = startRow; i < endRow; i++) {
            for (int j = 0; j < cols; j++) {
                if (nextGen[i][j] == 1) allDead = false;
                if (nextGen[i][j] != currentGen[i][j]) noChange = false;
            }
        }

        myMsg.type = GENDONE;
        if (allDead) myMsg.type = ALLDEAD;
        else if (noChange) myMsg.type = NOCHANGE;
        allDead = true; // Reset for next generation
        noChange = true; // Reset for next generation

        myMsg.iSender = iMe;
        myMsg.value1 = 0; // Placeholder, not used in this context
        myMsg.value2 = 0; // Placeholder, not used in this context
        sendMsg(0, &myMsg); // Acknowledge RANGE message
    }
    myMsg.iSender = iMe;
    myMsg.type = ALLDONE;
    myMsg.value1 = 0; // Placeholder, not used in this context
    myMsg.value2 = 0; // Placeholder, not used in this context
    sendMsg(0, &myMsg); // Notify main thread of completion

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <number of threads> <input file> <number of generations> (optional)<print>\n";
        return 1;
    }
    if (atoi(argv[1]) < 1 || atoi(argv[1]) > MAXTHREAD) {
        cerr << "Error: number of threads must be between 1 and " << MAXTHREAD << endl;
        return 1;
    }
    if (atoi(argv[3]) < 1) {
        cerr << "Error: number of generations must be at least 1" << endl;
        return 1;
    } else {
        gen = atoi(argv[3]);
    }
    if (5 == argc && strcmp(argv[4], "y") == 0) print = true;

    int numThreads = min(max(1, atoi(argv[1])), MAXTHREAD);
    cout << "Number of threads: " << numThreads << endl;

    // Read input file and initialize genEven and genOdd
    int rows = 0, cols = 0;
    // Assuming the input file is formatted correctly
    FILE* inputFile = fopen(argv[2], "r");
    if (!inputFile) {
        cerr << "Error opening file: " << argv[2] << endl;
        return 1;
    }
    char line[(MAXGRID * 2) + 2]; // +2 for newline and null terminator
    char newLine[MAXGRID + 2];
    while (fgets(line, sizeof(line), inputFile) != nullptr) {
        rows++;
        int idx = 0;
        for (int j = 0; j < strlen(line); j++) {
            if (line[j] == '1' || line[j] == '0') {
                newLine[idx++] = line[j];
            }
        }
        newLine[idx] = '\0'; // Null-terminate the newLine string
        cols = idx; // Update cols to the current line's length
    }
    fseek(inputFile, 0, SEEK_SET); // Reset file pointer to the beginning
    for (int i = 0; i < rows; i++) {
        genEven = (int **) realloc(genEven, (i + 1) * sizeof(int *));
        genOdd = (int **) realloc(genOdd, (i + 1) * sizeof(int *));
        genEven[i] = (int *) malloc(cols * sizeof(int));
        genOdd[i] = (int *) malloc(cols * sizeof(int));
        fgets(line, sizeof(line), inputFile);
        int idx = 0;
        for (int j = 0; j < strlen(line); j++) {
            if (line[j] == '1' || line[j] == '0') {
                genEven[i][idx++] = line[j] - '0'; // Convert char to int
            }
        }
        // Initialize genOdd to 0
        for (int j = 0; j < cols; j++) {
            genOdd[i][j] = 0;
        }
    }
    cout << "Grid dimensions: " << rows << " rows, " << cols << " columns\n";

    numThreads = min(numThreads, rows); // Limit threads to number of rows
    fclose(inputFile);
    pthread_t t_ids[numThreads];

    for (int i = 0; i <= numThreads; i++) {
        if(sem_init(&semSend[i], 0, 1) < 0) {
            cerr << "Error initializing semaphore " << i << endl;
            exit(1);
        }
        if(sem_init(&semRecv[i], 0, 0) < 0) {
            cerr << "Error initializing semaphore " << i << endl;
            exit(1);
        }
    }

    cout << "Semaphores initialized\n";

    int rangePerThread = floor((double) rows / numThreads);
    //cout << "Each thread will process approximately " << rangePerThread << " rows\n";
    int start = 0;
    int end = rangePerThread;
    msg myMsg;
    for(int i = 1; i <= numThreads; i++) {
        // Send initial RANGE message to each thread
        myMsg.iSender = 0;
        myMsg.type = RANGE;
        myMsg.value1 = start;
        myMsg.value2 = (i == numThreads) ? rows : end; // Last thread takes the remainder
        //cout << "Sending RANGE to thread " << i << ": rows " << myMsg.value1 << " to " << myMsg.value2 << endl;
        sendMsg(i, &myMsg);
        //cout << "RANGE message sent to thread " << i << endl;
        start = end;
        end += rangePerThread;
    }

    //cout << "Initial RANGE messages sent to threads\n";
    if (print) {
        cout << "Initial Generation:\n";
        printGen(genEven, rows, cols, 0);
    }

    int thread_ids[numThreads];
    for (int i = 1; i <= numThreads; i++) {
        thread_ids[i] = i;
        //cout << "Creating thread " << i << endl;
        pthread_create(&t_ids[i], nullptr, life, (void *) &thread_ids[i]);
        msg initMsg;
        initMsg.iSender = 0;
        initMsg.type = GO;
        initMsg.value1 = rows; // Rows
        initMsg.value2 = cols; // Columns
        sendMsg(i, &initMsg); // Start the thread processing
        //cout << "GO message sent to thread " << i << endl;
    }

    //cout << "Threads created\n";

    // Run for the specified number of generations
    int gendoneCount = 0;
    int allDoneCount = 0;
    int genReplies[numThreads]; // Track replies from each thread
    bool simulationEnd = true, continueSimulation = true;
    while (gendoneCount < gen && continueSimulation) {
        msg replyMsg;
        for (int threadsDone = 0; threadsDone < numThreads;) {
            recvMsg(0, &replyMsg);
            genReplies[replyMsg.iSender - 1] = replyMsg.type;
            if (replyMsg.type == GENDONE || replyMsg.type == ALLDEAD || replyMsg.type == NOCHANGE) {
                threadsDone++;
            }
            // Check for ALLDEAD or NOCHANGE conditions
            int firstType = genReplies[0];
            for (int i = 1; i < numThreads; i++) {
                if (genReplies[i] != firstType) {
                    simulationEnd = false;
                    break;
                }
            }
            if (simulationEnd && (firstType == ALLDEAD || firstType == NOCHANGE)) {
                cout << "All threads reported " << (firstType == ALLDEAD ? "ALLDEAD" : "NOCHANGE") << ". Ending simulation early.\n";
                continueSimulation = false; // Force exit of outer loop
                // Send STOP message to all threads
                for (int i = 1; i <= numThreads; i++) {
                    msg stopMsg;
                    stopMsg.iSender = 0;
                    stopMsg.type = STOP;
                    stopMsg.value1 = 0;
                    stopMsg.value2 = 0;
                    sendMsg(i, &stopMsg);
                }
                break;
            } else {
                simulationEnd = true; // Reset for next check
            }
        }
        gendoneCount++;
        // Print the current generation if required
        if (print) {
            if (gendoneCount % 2 == 1) printGen(genOdd, rows, cols, gendoneCount);
            else printGen(genEven, rows, cols, gendoneCount);
        }
        // Send GO message to all threads for next generation
        for (int i = 1; i <= numThreads; i++) {
            msg goMsg;
            goMsg.iSender = 0;
            goMsg.type = GO;
            goMsg.value1 = rows; // Rows
            goMsg.value2 = cols; // Columns
            sendMsg(i, &goMsg);
        }
    }

    cout << "All threads have completed processing.\n";

    return 0;
}