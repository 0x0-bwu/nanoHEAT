#define NANO_UNIT_TEST
#include <boost/test/included/unit_test.hpp>
#include <boost/stacktrace.hpp>

#include "TestPackage.hpp"
#include "TestModel.hpp"
using namespace boost::unit_test;
extern test_suite * create_nano_package_test_suite();
extern test_suite * create_nano_heat_model_test_suite();
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
    framework::master_test_suite().add(create_nano_package_test_suite());
    framework::master_test_suite().add(create_nano_heat_model_test_suite());
    return 0;
}