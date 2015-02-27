#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>

#include <distributed_locking/DLM.hpp>

#include "TestHelper.hpp"

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

BOOST_AUTO_TEST_SUITE(ricart_agrawala_extended)

/**
 * Test correct reactions if an agent does not respond PROBE messages.
 *
 */
BOOST_AUTO_TEST_CASE(non_responding_agent)
{
    BOOST_TEST_MESSAGE("ricart_agrawala_extended_non_responding_agent");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 3 Agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLM::Ptrs
    // Only a1 holds a resource
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA_EXTENDED, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA_EXTENDED, a2, std::vector<std::string>());
    DLM::Ptr dlm3 = DLM::create(protocol::RICART_AGRAWALA_EXTENDED, a3, std::vector<std::string>());

    // AgentID 3 locks
    BOOST_REQUIRE_THROW(dlm3->lock(rsc1, boost::assign::list_of(a1)(a2)), std::invalid_argument);

    dlm3->discover(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));

    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    
    // AgentID 2 tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
   
    // Now, agent 3 dies (it did not respond to a2 yet, as it still holds the lock)
    // We sleep 6s (1s more than the threshold) and call the trigger() method again
    for(int i = 0; i < 7; ++i)
    {
        BOOST_TEST_MESSAGE("Trigger");

        dlm1->trigger();
        dlm2->trigger();
        forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

        sleep(1);
    }

    // A2 should now have obtained the lock, as a3 was not important
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);
 
    // He unlocks
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    
    // A1 locks
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    
    // A2 tries to lock again
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // Now, agent1 dies (it did not respond a2 yet, as it still holds the lock)
    // Now, agent 3 dies (it did not respond to a2 yet, as it still holds the lock)
    // We sleep 6s (1s more than the threshold) and call the trigger() method again
    for(int i = 0; i < 10; ++i)
    {
        BOOST_TEST_MESSAGE("Trigger");
        dlm2->trigger();
        forwardAllMessages(boost::assign::list_of(dlm2));
        sleep(1);
    }
    
    // a1 was owner of rsc1, so it should be considered important.
    // therefore, a2 should mark the resource as unobtainable
    BOOST_CHECK_MESSAGE(dlm2->getLockState(rsc1) == lock_state::UNREACHABLE, "Lock state is " << dlm2->getLockState(rsc1));
    
    // Calling lock now should trigger an exception
    BOOST_CHECK_THROW(dlm2->lock(rsc1, boost::assign::list_of(a1)), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
