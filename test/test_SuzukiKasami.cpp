#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <distributed_locking/DLM.hpp>

#include <iostream>

#include "TestHelper.hpp"

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

BOOST_AUTO_TEST_SUITE(suzuki_kasami)

/**
 * Test taken from the ruby script.
 */
BOOST_AUTO_TEST_CASE(test_from_ruby_script)
{
    BOOST_TEST_MESSAGE("suzuki_kasami/test_from_ruby_script");

    // Create 3 Agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 3 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::SUZUKI_KASAMI, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::SUZUKI_KASAMI, a2, std::vector<std::string>());
    DLM::Ptr dlm3 = DLM::create(protocol::SUZUKI_KASAMI, a3, std::vector<std::string>());

    // Now we lock
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // And check it is being locked
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // AgentID two tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1)(a3));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    // AgentID 1 release
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Check it is being locked by a2 now
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);

    // A3 locks, sleep, A1 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    
    // Unlock and see that he is NOT_INTERESTED
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    
    // Since the algorithms is not fair and a1 comes first in a2's list
    // he gets the token first.
    // Check it is being locked by a1 now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    // AgentID 1 releasese
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Check it is being locked by a3 now
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::LOCKED);

    // AgentID 3 release
    dlm3->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));    

    // Check no one is interested any more
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}

/**
 * Simple test with 3 agents, where a1 requests, obtains, and releases the lock for a resource.
 */
BOOST_AUTO_TEST_CASE(suzuki_kasami_basic_hold_and_release)
{
    std::cout << "suzuki_kasami_basic_hold_and_release" << std::endl;
    // Create 3 Agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 3 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::SUZUKI_KASAMI, a1, std::vector<std::string>());
    DLM::Ptr dlm2 = DLM::create(protocol::SUZUKI_KASAMI, a2, rscs);
    DLM::Ptr dlm3 = DLM::create(protocol::SUZUKI_KASAMI, a3, std::vector<std::string>());
    
    // dlm1 should not be interested in the resource.
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    // He should be INTERESTED now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    
    // Now, as a1 already owns the token, calling lock should work immediately, without any messages being sent
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
   
    // We release again
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
}

/**
 * Two agents want the same resource. A2 wants it while
 * a1 holds the lock.
 */
BOOST_AUTO_TEST_CASE(two_agents_conflict)
{
    BOOST_TEST_MESSAGE("suzuki_kasami/two_agents_conflict");

    // Create 2 Agents
    AgentID a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);
    
    // Create 2 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::SUZUKI_KASAMI, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::SUZUKI_KASAMI, a2, std::vector<std::string>());
    
    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    
    // Now a2 tries to lock the same resource
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    
    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    
    // Now, agent2 should hold the lock
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);
    
    // Now a2 releases the lock
    dlm2->unlock(rsc1);
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
}

BOOST_AUTO_TEST_SUITE_END()
