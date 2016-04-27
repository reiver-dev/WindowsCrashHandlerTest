#include <Windows.h>
#include <csignal>
#include <thread>
#include <cassert>
#include <stdexcept>



void _sigsegvhandler(int sig)
{
    // _pxcptinfoptrs <- exception information pointers
    printf("SIGSEGV HANDLED\n");
    exit(1);
}

void _sigaborthandler(int sig)
{
    printf("SIGABRT HANDLED\n");
    exit(1);
}

void set_signal_handlers()
{
    std::signal(SIGSEGV, _sigsegvhandler);
    std::signal(SIGABRT, _sigaborthandler);
}

//////////////////////////////////

LONG NTAPI my_vectored_handler(_EXCEPTION_POINTERS *exception)
{
    printf("Vectored exception handler\n");
    exit(1);

    return EXCEPTION_CONTINUE_SEARCH;
}

void set_vectored_handler(int val)
{
    AddVectoredExceptionHandler(val, my_vectored_handler);
}

//////////////////////////////////

LONG WINAPI my_unhandled_excetion_filter(_EXCEPTION_POINTERS *exception)
{
    printf("Unhandled exception handler\n");
    fflush(stdout);
    exit(1);

    return EXCEPTION_CONTINUE_SEARCH;
}

void set_unhandled_exception_filter()
{
    SetUnhandledExceptionFilter(my_unhandled_excetion_filter);
}


/////////////////////////////////


const char *USAGE = \
R"x(
  -t --thread         call crash function in other thread
     --seh            __try __except SEH block

  -s --segfault       call segfault
  -a --abort          call abort
  -p --pure           pure virtual function call
     --cppu           throw and not handle cpp exception
     --cpph           throw and handle cpp exception
  
     --vectored0      use windows vectored exception handler without first flag
     --vectored1      use windows vectored exception handler with first flag
     --suhf           use SetUnhandledExceptionFilter
     --signal         use signal handler

)x";


enum class ErrorType : int {
    NONE, SEGFAULT, ABORT, CPP_UNHANDLED, CPP_HANDLED, PVC
};

enum class CatchMethod : int {
    NONE, VECTORED0, VECTORED1, SUHF, SIGNAL
};

struct Args {
    bool threading = false;
    bool seh = false;
    ErrorType error = ErrorType::NONE;
    CatchMethod catch_method = CatchMethod::NONE;
};

bool compare_arg(const char *value, const char *arg0)
{
    return !strcmp(value, arg0);
}

template<typename... ARGV>
bool compare_arg(const char *value, const char *arg0, ARGV&&... argn)
{
    return compare_arg(value, arg0) || compare_arg(value, argn...);
}



Args parse_arguments(int argc, char *argv[])
{
    Args result;

    for (size_t arg = 0, len = argc; arg < len; ++arg) {
        auto val = argv[arg];
        if (compare_arg(val, "-t", "--thread")) {
            result.threading = true;
        } else if (compare_arg(val, "--seh")) {
            result.seh = true;
        } else if (compare_arg(val, "-s", "--segfault")) {
            result.error = ErrorType::SEGFAULT;
        } else if (compare_arg(val, "-a", "--abort")) {
            result.error = ErrorType::ABORT;
        } else if (compare_arg(val, "-p", "--pure")) {
            result.error = ErrorType::PVC;
        } else if (compare_arg(val, "--cppu")) {
            result.error = ErrorType::CPP_UNHANDLED;
        } else if (compare_arg(val, "--cpph")) {
            result.error = ErrorType::CPP_HANDLED;
        } else if (compare_arg(val, "--vectored0")) {
            result.catch_method = CatchMethod::VECTORED0;
        } else if (compare_arg(val, "--vectored1")) {
            result.catch_method = CatchMethod::VECTORED1;
        } else if (compare_arg(val, "--suhf")) {
            result.catch_method = CatchMethod::SUHF;
        } else if (compare_arg(val, "--signal")) {
            result.catch_method = CatchMethod::SIGNAL;
        }
    }

    return result;
}


class A {
public:

    A()
    {
        init_internal();
    }

    void init_internal()
    {
        init();
    }

    virtual ~A() {}
   
    virtual void init() = 0;
    
};

class B : public A {
public:
    B() { }
    ~B() { }

    void init() override
    {

    }

};  


int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf(USAGE);
        exit(1);
    }

    auto args = parse_arguments(argc, argv);

    switch (args.catch_method) {
    case CatchMethod::NONE:
        break;
    case CatchMethod::SIGNAL:
        set_signal_handlers();
        break;
    case CatchMethod::VECTORED0:
        set_vectored_handler(FALSE);
        break;
    case CatchMethod::VECTORED1:
        set_vectored_handler(TRUE);
        break;
    case CatchMethod::SUHF:
        set_unhandled_exception_filter();
        break;
    default:
        exit(2);
    }

    std::function<void()> routine;

    switch (args.error) {
    case ErrorType::NONE:
        printf("Specify crash method!\n");
        exit(2);
        break;
    case ErrorType::SEGFAULT:
        routine = []() {
            printf("Segfault inbound\n");
            volatile int *val = nullptr;
            int res = *val;
            printf("Segfault performed\n");
        };
        break;
    case ErrorType::ABORT:
        routine = []() {
            printf("Abort inbound\n");
            assert(false);
            printf("Abort performed\n");
        };
        break;
    case ErrorType::CPP_UNHANDLED:
        routine = []() {
            printf("C++ unhandled exception inbound\n");
            throw std::runtime_error("This is unhandled exception");
            printf("C++ unhandled exception performed\n");
        };
        break;
    case ErrorType::CPP_HANDLED:
        routine = []() {
            try {
                printf("C++ exception inbound\n");
                throw std::runtime_error("This is unhandled exception");
            } catch (std::exception &) {
                printf("C++ exception handler\n");
            }
            printf("C++ exception performed\n");
        };
        break;
    case ErrorType::PVC:
        routine = []() {
            printf("Pure virtual call inbound\n");
            B();
            printf("Pure virtual call performed\n");
        };
        break;
    default:
        exit(2);
    }

    if (args.seh) {
        routine = [routine]()
        {
            __try {
                routine();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                printf("Epic SEH Handler\n");
            }
        };
    }

    if (args.threading) {
        std::thread t(routine);
        t.join();
    } else {
        routine();
    }

    return 0;
}