#include <iostream>

#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

void handler(int aSignal, siginfo_t *aSigInfo, void *extraInfo)
{
        void *btArray[128];
        int btSize;
        char** btSymbols;

        std::cout << std::endl << "########## Backtrace ##########" << std::endl;
        btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *));
        std::cout << "Number of elements in backtrace: " <<  btSize << std::endl;

        if (btSize > 0) {

            btSymbols = backtrace_symbols(btArray, btSize);
            if (btSymbols) {
                for (int i = btSize - 1; i >= 0; --i) {
                    std::cout << btSymbols[i] << std::endl;
                }
            }
        }

        std::cout << "Handler done." << std::endl;
}

void g()
{
        abort();
}

void f()
{
        g();
}

int main(int argc, char **argv)
{
        struct sigaction SignalAction;

        SignalAction.sa_sigaction = handler;
        sigemptyset(&SignalAction.sa_mask);
        SignalAction.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &SignalAction, NULL);
        sigaction(SIGABRT, &SignalAction, NULL);

        f();
}


