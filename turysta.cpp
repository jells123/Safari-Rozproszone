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
#include "constants.h"
#include "handlers.h"

using namespace std;

MPI_Status status;

enum Role {
    UNKNOWN,
    ORG, // organizator
    TUR // turysta
};
Role currentRole;

typedef struct processInfo {
    Role role;
    int value;
} processInfo;

int T = 10; // liczba turystow
int G = 2; // rozmiar grupy
int P = 3; // liczba przewodnikow

int MAX_ORGS;

pthread_mutex_t tab_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inviteResponses_mtx = PTHREAD_MUTEX_INITIALIZER;

vector<processInfo> tab; // T == size??
vector<int> permissions, queue;

int inviteResponses;
void *receiveMessages(void *ptr) {

    packet pkt;
    while ( true ) {

        //println("czekam na wiadomości...\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (pkt.type != NOT_ORG)
            println("wiadomość: %s od %d\n", msgTypes[pkt.type], status.MPI_SOURCE);
        for (int i = 0; i < handlers.size(); i++) {
            if (handlers[i].msgType == pkt.type)
                handlers[i].handler( &pkt, status.MPI_SOURCE ); 
        }

        /*
        packet *newpkt = (packet*) malloc(sizeof(packet));
        memcpy(newpkt, (const char *)&pkt, sizeof(packet));
        push_pkt(newpkt, status);
        free(newpkt);
        */
    }

    return (void *)0;
}

void randomRole() {

    Role prevRole = currentRole;
    int czy_organizator = rand() % 100;
    if (czy_organizator < ORG_PROBABILITY)
        currentRole = ORG;
    else 
        currentRole = TUR;

    if (prevRole != currentRole) {
        println("new role: %s\n", rolesNames[currentRole]);

        if (currentRole == TUR) {
            packet msg = { timestamp, NOT_ORG, 0 };
            for (int i = 0; i < size; i++)
                if (i != tid)
                    MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        }
    }
}

vector<int> myGroup;
vector<int> invitations;
void *sendMessages(void *ptr) { // NOPE, MYLĄCA NAZWA FUKNCJI ;)

    packet pkt;
    while ( true ) {

        if (currentRole == ORG) {

            while (myGroup.size() != G) {

                pthread_mutex_lock(&inviteResponses_mtx);
                inviteResponses = 0;
                invitations.clear();
                pthread_mutex_unlock(&inviteResponses_mtx);

                vector<int>::iterator it;
                int choice, missing = G - myGroup.size();

                for ( int i = 0; i < G; ++i ) {
                //while (invitations.size() != G) {
                    do {
                        choice = rand()%T;
                        it=find(invitations.begin(), invitations.end(), choice);
                    } while (choice == tid || it != invitations.end());
                    invitations.push_back(choice);
                }
                //}

                //for (auto i : invitations) {
                for (int i = 0; i < G; ++i) {
                    int idx = invitations.size() - 1 - i;
                    packet msg = { timestamp, INVITE, missing };
                    MPI_Send( &msg, 1, MPI_PAKIET_T, idx, MSG_TAG, MPI_COMM_WORLD);
                }

                while (inviteResponses != invitations.size()) ;

            }

            println("I've got a group!\n");
            
        }

    }

    return (void *)0;

}

   // ACCEPT, // przyjęcie zaproszenia 
   //  REJECT_HASGROUP, // odmowa - mam już grupę
   //  REJECT_ISORG, // odmowa - jestem organizatorem

void inviteHandler(packet *pkt, int src) {
    if (currentRole == TUR && myGroup.empty()) {
        myGroup.push_back(src);
        packet msg = { timestamp, ACCEPT, 0 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);   
    }
}

void not_orgHandler(packet *pkt, int src) {
    tab[src].role = TUR;
    int touristsCount = 0;
    for (int i = 0; i < size; i++) {
        if (tab[i].role == TUR)
            touristsCount++;
    }
    if (currentRole == TUR
        && T - touristsCount < MAX_ORGS 
        && MAX_ORGS - (T - touristsCount) > tid ) { // o jeden za mało?
        currentRole = ORG;
        println("I became ORG! Because I could.\n");
    }
}

void acceptHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    pthread_mutex_unlock(&inviteResponses_mtx);

    myGroup.push_back(src);
    tab[src].role = TUR;
    tab[src].value = tid;
}

void reject_hasgroupHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    pthread_mutex_unlock(&inviteResponses_mtx);

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;

}

void reject_isorgHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    pthread_mutex_unlock(&inviteResponses_mtx);

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;

}


void prepare() {

    tab.reserve(T);
    permissions.reserve(G);
    queue.reserve((int) T/G);
    timestamp = 0;

    currentRole = UNKNOWN;
    randomRole();

    for (int i = 0; i < T; i++) {
        tab[i].role = UNKNOWN;
        tab[i].value = -1;
    }

    addMessageHandler(NOT_ORG, not_orgHandler);
    addMessageHandler(ACCEPT, acceptHandler);
    addMessageHandler(REJECT_HASGROUP, reject_hasgroupHandler);
    addMessageHandler(REJECT_ISORG, reject_isorgHandler);
    addMessageHandler(INVITE, inviteHandler);

}

int main(int argc, char * * argv) {

    if (argc == 4) {
        T = atoi(argv[1]);
        G = atoi(argv[2]);
        P = atoi(argv[3]);
    }
    MAX_ORGS = T / G;

    init(&argc, &argv);
    //cout << "Liczba turystow: " << T << " Wielkosc grupy: " << G << " Liczba przewodnikow: " << P << endl;

    prepare();

    pthread_create( &sender_th, NULL, sendMessages, 0);
    pthread_create( &receiver_th, NULL, receiveMessages, 0);

    packet msg;

    while (true) {
    }

    MPI_Finalize();
}