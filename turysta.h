#ifndef TURYSTAH
#define TURYSTAH

#include <stdlib.h> 
#include <stdio.h> 
#include <mpi.h> 

#include <time.h> 
#include <iostream> 
#include <cstdlib> 
#include <vector>
#include <set>
#include <algorithm>

#include "packet.h"
#include "inits.h"
//#include "constants.h"
#include "handlers.h"

using namespace std;

enum Role {
    UNKNOWN,
    ORG, // organizator
    TUR // turysta
};

typedef struct processInfo {
    Role role;
    int value;
} processInfo;

typedef struct orgInfo {
    int timestamp;
    int tid;
} orgInfo;

extern Role currentRole;
extern MPI_Status status;
extern int T, G, P, MAX_ORGS;
extern int inviteResponses, missing, permissions;

extern pthread_mutex_t tab_mtx, inviteResponses_mtx, myGroup_mtx, timestamp_mtx, queue_mtx, permission_mtx;
extern pthread_cond_t inviteResponses_cond, permission_cond;

extern vector<processInfo> tab; // T == size??
extern vector<orgInfo> queue;
extern vector<int> reqPermissions, myGroup, invitations;

extern int ROOT, MSG_TAG, ORG_PROBABILITY;

void *receiveMessages(void *ptr);
void deleteFromQueue(int tid);
void reserveGuide(); 
int tabSummary();
void comeBack(); 

void orgsDeadlockProcess();
void *orgThreadFunction(void *ptr);
void randomRole(); 
void prepare();

int main(int argc, char * * argv);

#endif