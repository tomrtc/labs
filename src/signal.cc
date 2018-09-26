#include <iostream>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

static void* signalHandlerThread(void* arg)
{
    // Use signal mask provided by main().
    // The signals we want to catch with sigwait() must already be blocked.
    auto signals = static_cast<sigset_t *>(arg);

    auto count = 0;
    auto terminateRequested = false;

    while (!terminateRequested) {
        int signalNumber = 0;
        auto status = sigwait(signals, &signalNumber);

        if (status == 0) {
            switch (signalNumber) {
                case SIGHUP:
                    std::cout << ++count << ": Received HUP" << std::endl;
                    break;

                case SIGUSR1:
                    std::cout << ++count << ": Received USR1" << std::endl;
                    break;

                case SIGUSR2:
                    std::cout << ++count << ": Received USR2" << std::endl;
                    break;

                case SIGTERM:
                    std::cout << ++count << ": Received TERM" << std::endl;
                    terminateRequested = true;
                    break;

                case SIGINT:
                    std::cout << ++count << ": Received INT" << std::endl;
                    terminateRequested = true;
                    break;

                default:
                    std::cout << "Received signal number " << signalNumber << std::endl;
                    break;
            }
        }
        else {
            std::cerr << "sigwait() returned " << status;
        }
    }

    return nullptr;
}

int main(int argc, const char* argv[])
{
    // Set signal mask to block any signals we want to catch.
    // Any spawned threads will inherit this signal mask.
    auto signalNumbers = { SIGHUP, SIGUSR1, SIGUSR2, SIGTERM, SIGINT };
    sigset_t signals;
    sigemptyset(&signals);
    for (auto signalNumber: signalNumbers) {
        sigaddset(&signals, signalNumber);
    }
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    // Spawn our signal-handling thread, passing it our set of blocked signals
    pthread_t thread;
    pthread_create(&thread, nullptr, signalHandlerThread, &signals);

    // Tell user how to send the signals to us
    std::cerr << "sigtest waiting for signals; run this command:\n  kill -HUP "
              << getpid()
              << "\n  or -USR1|-USR2|-TERM|-INT"
              << std::endl;

    // Wait for the signal-handling thread to terminate
    void *threadResult = nullptr;
    pthread_join(thread, &threadResult);

    std::cerr << "<Program exit>" << std::endl;
    return 0;
}
