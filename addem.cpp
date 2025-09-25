#include <iostream>
using namespace std;
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <semaphore.h>


#define MAXTHREAD 10
#define RANGE 1
#define ALLDONE 2

struct msg {
    int iSender; /* ID of the sender thread (0..number of threads) */
    int type; /* type of message */
    int value1; /* first value */
    int value2; /* second value */
};

msg MAILBOXES[MAXTHREAD + 1]; /* mailboxes for threads 0..MAXTHREAD */
sem_t semSend[MAXTHREAD + 1];
sem_t semRecv[MAXTHREAD + 1];

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

void *adder(void *arg) {
    int iMe = *((int *) arg);
    int iSum = 0;
    msg myMsg, replyMsg;


    recvMsg(iMe, &replyMsg);
    if (replyMsg.type == ALLDONE) {
        iSum += replyMsg.value1;
    } else if (replyMsg.type == RANGE) {
        for (int j = replyMsg.value1; j <= replyMsg.value2; j++) {
            iSum += j;
        }
    }
    myMsg.iSender = iMe;
    myMsg.type = ALLDONE;
    myMsg.value1 = iSum;
    sendMsg(0, &myMsg);

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <number of threads> <value for summation>\n";
        return 1;
    }

    int numThreads = min(max(1, atoi(argv[1])), MAXTHREAD);
    int value = atoi(argv[2]);
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

    int rangePerThread = value / numThreads;
    int start = 1;
    int end = rangePerThread;
    msg myMsg;
    for(int i = 1; i <= numThreads; i++) {
        // Send initial RANGE message to each thread
        myMsg.iSender = 0;
        myMsg.type = RANGE;
        myMsg.value1 = start;
        myMsg.value2 = (i == numThreads) ? value : end; // Last thread takes the remainder
        sendMsg(i, &myMsg);
        start = end + 1;
        end += rangePerThread;
    }

    int thread_ids[numThreads];
    for (int i = 0; i <= numThreads; i++) {
        thread_ids[i] = i;
        pthread_create(&t_ids[i], nullptr, adder, (void *) &thread_ids[i]);
    }


    // Retrieve results from threads
    int totalSum = 0;
    msg replyMsg;
    for (int i = 0; i < numThreads; i++) {
        recvMsg(0, &replyMsg);
        if (replyMsg.type == ALLDONE) {
            totalSum += replyMsg.value1;
        }
    }

    cout << "All messages received\n";

    for (int i = 0; i < numThreads; i++) {
        pthread_join(t_ids[i], nullptr);
    }

    cout << "All threads joined\n";
    cout << "Total sum = " << totalSum << endl;

    return 0;
}