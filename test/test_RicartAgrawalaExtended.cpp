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

/**
 * Test correct reactions if an agent does not respond PROBE messages.
 */
BOOST_AUTO_TEST_CASE(ricart_agrawala_extended_non_responding_agent)
{
    std::cout << "ricart_agrawala_extended_non_responding_agent" << std::endl;

    // Create 3 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::RICART_AGRAWALA_EXTENDED, a1, rscs);
    DLM* dlm2 = DLM::dlmFactory(protocol::RICART_AGRAWALA_EXTENDED, a2, std::vector<std::string>());
    DLM* dlm3 = DLM::dlmFactory(protocol::RICART_AGRAWALA_EXTENDED, a3, std::vector<std::string>());

    // Agent 3 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    
    // Agent 2 tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    
    // Now, agent 3 dies (it did not respond a2 yet, as it still holds the lock)
    dlm2->trigger();
    // We sleep 6s (1s more than the threshold) and call the trigger() method again
    boost::this_thread::sleep_for(boost::chrono::seconds(6));
    dlm2->trigger();
    
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
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));

    // Now, agent1 dies (it did not respond a2 yet, as it still holds the lock)
    dlm2->trigger();
    // We sleep 6s (1s more than the threshold) and call the trigger() method again
    boost::this_thread::sleep_for(boost::chrono::seconds(6));
    dlm2->trigger();
    
    // a1 was owner of rsc1, so he should be considered important.
    // therefore, a2 should mark the resource as unobtainable
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::UNREACHABLE);
    
    // Calling lock now should trigger an exception
    BOOST_CHECK_THROW(dlm2->lock(rsc1, boost::assign::list_of(a1)), std::runtime_error);
}
