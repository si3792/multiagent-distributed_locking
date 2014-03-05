#include <boost/test/unit_test.hpp>
#include <ricart_agrawala/Dummy.hpp>

using namespace ricart_agrawala;

BOOST_AUTO_TEST_CASE(it_should_not_crash_when_welcome_is_called)
{
    ricart_agrawala::DummyClass dummy;
    dummy.welcome();
}
