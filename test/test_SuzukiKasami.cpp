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

/**
 * Test taken from the ruby script.
 */
BOOST_AUTO_TEST_CASE(suzuki_kasami_test_from_ruby_script)
{
    std::cout << "suzuki_kasami_test_from_ruby_script" << std::endl;
    // Create 3 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a1);
    DLM* dlm2 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a2);
    DLM* dlm3 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a3);

    // Define critical resource
    std::string rsc1 = "resource";
    // Agent1 owns the resource (unnecesary in RA)
    dlm1->setOwnedResources(boost::assign::list_of(rsc1));

    // Now we lock
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // And check it is being locked
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // Agent two tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1)(a3));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    // Agent 1 release
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
    // Agent 1 releasese
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Check it is being locked by a3 now
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::LOCKED);

    // Agent 3 release
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
    Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a1);
    DLM* dlm2 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a2);
    DLM* dlm3 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a3);

    // Define critical resource
    std::string rsc1 = "rsc1";
    // Let a2 hold it at the beginning
    dlm2->setOwnedResources(boost::assign::list_of(rsc1));
    
    // dlm1 should not be interested in the resource.
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    // He should be INTERESTED now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);
    
    // OutgoingMessages should contain one messages (2 receivers)
    BOOST_CHECK(dlm1->hasOutgoingMessages());
    ACLMessage dlm1msg = dlm1->popNextOutgoingMessage();
    BOOST_CHECK(!dlm1->hasOutgoingMessages());
    BOOST_REQUIRE_EQUAL(2, dlm1msg.getAllReceivers().size());

    // Lets foward this mail to our agents
    dlm2->onIncomingMessage(dlm1msg);
    dlm3->onIncomingMessage(dlm1msg);

    // A2 should now have responded to agent1
    BOOST_CHECK(dlm2->hasOutgoingMessages());
    BOOST_CHECK(!dlm3->hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2->popNextOutgoingMessage();
    BOOST_CHECK(!dlm2->hasOutgoingMessages());

    // Lets forward the responses
    dlm1->onIncomingMessage(dlm2msg);

    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    
    // Now, as a1 already owns the token, calling lock should work immediately, without any messages being sent
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    BOOST_CHECK(!dlm1->hasOutgoingMessages());
   
    // We release again
    dlm1->unlock(rsc1);
}

/**
 * Two agents want the same resource. A2 wants it while
 * a1 holds the lock.
 */
BOOST_AUTO_TEST_CASE(suzuki_kasami_two_agents_conflict)
{
    std::cout << "suzuki_kasami_two_agents_conflict" << std::endl;
    // Create 2 Agents
    Agent a1 ("agent1"), a2 ("agent2");
    // Create 2 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a1);
    DLM* dlm2 = DLM::dlmFactory(protocol::SUZUKI_KASAMI, a2);

    // Define critical resource
    std::string rsc1 = "rsc1";
    // Let a1 hold it at the beginning
    dlm1->setOwnedResources(boost::assign::list_of(rsc1));
    
    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    
    // Now a2 tries to lock the same resource
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    // OutgoingMessages should contain one messages
    BOOST_CHECK(dlm2->hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2->popNextOutgoingMessage();
    BOOST_CHECK(!dlm2->hasOutgoingMessages());

    // Lets foward this mail
    dlm1->onIncomingMessage(dlm2msg);
    
    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    // Now agent1 should have a new outgoing message with the token
    BOOST_CHECK(dlm1->hasOutgoingMessages());
    ACLMessage dlm1rsp = dlm1->popNextOutgoingMessage();
    BOOST_CHECK(!dlm1->hasOutgoingMessages());
    
    // We forward it
    dlm2->onIncomingMessage(dlm1rsp);
    // Now, agent2 should hold the lock
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);
    
    // Now a2 releases the lock, and should have a new outgoing message, which we forward
    dlm2->unlock(rsc1);
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}
