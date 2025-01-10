#define NANO_UNIT_TEST
#include <boost/test/included/unit_test.hpp>
#include <boost/stacktrace.hpp>

using namespace boost::unit_test;

void t_additional()
{
    //add additional test here
    BOOST_CHECK(true);
}

void SignalHandler(int signum)
{
    ::signal(signum, SIG_DFL);
    std::cout << boost::stacktrace::stacktrace();
    ::raise(SIGABRT);
}

test_suite * init_unit_test_suite(int argc, char* argv[])
{
    ::signal(SIGSEGV, &SignalHandler);
    ::signal(SIGABRT, &SignalHandler);

    framework::master_test_suite().add(BOOST_TEST_CASE(&t_additional));
    return 0;
}