#include <iostream>
#include <random>
#include <cstdint>
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h> // O_RDONLY, O_WRONLY
#include <sys/stat.h> // mkfifo
#include <signal.h>
#include <sys/sem.h>

#include "utils.h"
#include "passenger.h"

void passengerSignalHandler(int signum)
{
        switch (signum)
        {
                case SIGNAL_OK:
                        syncedCout("Passenger " + std::to_string(getpid()) + ": Received signal OK\n");
                        break;
                case PASSENGER_IS_OVERWEIGHT:
                        syncedCout("Passenger " + std::to_string(getpid()) + ": Received signal PASSENGER_IS_OVERWEIGHT\n");
                        exit(0);
                default:
                        syncedCout("Passenger " + std::to_string(getpid()) + ": Received unknown signal\n");
                        break;
        }
}


void passengerProcess(size_t id, int semIDBaggageCtrl, int semIDSecGate, int semIDGate)
{
        syncedCout("Passenger process: " + std::to_string(id) + "\n");

        Passenger passenger(id);

        // set signal handler
        struct sigaction sa;
        sa.sa_handler = passengerSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGNAL_OK, &sa, NULL) == -1)
        {
                perror("sigaction");
                exit(1);
        }
        if (sigaction(PASSENGER_IS_OVERWEIGHT, &sa, NULL) == -1)
        {
                perror("sigaction");
                exit(1);
        }

        // TODO: run passenger tasks here

        // decrement semaphore
        syncedCout("Passenger: decrementing semaphore\n");
        sembuf dec = {0, -1, 0};
        if (semop(semIDBaggageCtrl, &dec, 1) == -1)
        {
                perror("semop");
                exit(1);
        }

        BaggageInfo baggageInfo;
        baggageInfo.mPid = getpid();
        baggageInfo.mBaggageWeight = passenger.getBaggageWeight();
        int fd = open("baggageControlFIFO", O_WRONLY);
        if (fd == -1)
        {
                perror("open");
                exit(1);
        }
        if (write(fd, &baggageInfo, sizeof(baggageInfo)) == -1)
        {
                perror("write");
                exit(1);
        }
        close(fd);

        syncedCout("Passenger released from baggage control\n");

        // INFO: passenger waits for security control to be free
        // INFO: passenger waits for plane to be ready
        // INFO: passenger thread ends
}

void spawnPassengers(size_t num, const std::vector<uint64_t> &delays, int semIDBaggageCtrl, int semIDSecGate, int semIDGate)
{
        pid_t pid = getpid();
        syncedCout("Spawn passengers\n");
        for (size_t i = 0; i < num; i++)
        {
                std::vector<pid_t> pids(1);
                pids[0] = pid;
                createSubprocesses(1, pids, {"passenger"});
                if (getpid() != pid)
                {
                        passengerProcess(i, semIDBaggageCtrl, semIDSecGate, semIDGate);
                        exit(0);
                }
                syncedCout("Waiting for " + std::to_string(delays[i]) + " ms\n");
                usleep(delays[i] * 1000);
        }

        syncedCout("All passengers created\n");

        exit(0); // WARNING: this is a temporary solution
}


Passenger::Passenger(uint64_t id)
        : mID(id), mIsAggressive(false)
{
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);

        // vip == true when random number < VIP_PROBABILITY * UINT64_MAX
        mIsVip = dis(gen) < VIP_PROBABILITY * UINT64_MAX;
        mType = dis(gen) < 0.5 * UINT64_MAX;
        mBaggageWeight = dis(gen) % MAX_BAGGAGE_WEIGHT;
        mHasDangerousBaggage = dis(gen) < DANGEROUS_BAGGAGE_PROBABILITY * UINT64_MAX;

        vCout("Passenger " + std::to_string(mID) + " created with the following properties:\n");
        vCout("VIP: " + std::to_string(mIsVip) + "\n");
        vCout("Type: " + std::to_string(mType) + "\n");
        vCout("Baggage weight: " + std::to_string(mBaggageWeight) + "\n");
        vCout("Aggressive: " + std::to_string(mIsAggressive) + "\n");
        vCout("Dangerous baggage: " + std::to_string(mHasDangerousBaggage) + "\n");
}

Passenger::Passenger(uint64_t id, bool isVip, bool type, uint64_t baggageWeight, bool hasDangerousBaggage)
        : mID(id), mIsVip(isVip), mType(type), mBaggageWeight(baggageWeight), mIsAggressive(false), mHasDangerousBaggage(hasDangerousBaggage)
{
        vCout("Passenger " + std::to_string(mID) + " created with the following properties:\n");
        vCout("VIP: " + std::to_string(mIsVip) + "\n");
        vCout("Type: " + std::to_string(mType) + "\n");
        vCout("Baggage weight: " + std::to_string(mBaggageWeight) + "\n");
        vCout("Aggressive: " + std::to_string(mIsAggressive) + "\n");
        vCout("Dangerous baggage: " + std::to_string(mHasDangerousBaggage) + "\n");
}

uint64_t Passenger::getID() const
{
        return mID;
}

bool Passenger::getIsVip() const
{
        return mIsVip;
}

bool Passenger::getType() const
{
        return mType;
}

uint64_t Passenger::getBaggageWeight() const
{
        return mBaggageWeight;
}

bool Passenger::getIsAggressive() const
{
        return mIsAggressive;
}

bool Passenger::getHasDangerousBaggage() const
{
        return mHasDangerousBaggage;
}
