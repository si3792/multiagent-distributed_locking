#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <distributed_locking/RicartAgrawala.hpp>

#include <iostream>

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

/**
 * Simple test with 3 agents, where a1 requests, obtains, and releases the lock for a resource
 */
BOOST_AUTO_TEST_CASE(basic_hold_and_release)
{
    // Create 3 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Create 3 DLMs
    RicartAgrawala dlm1 = RicartAgrawala(a1);
    RicartAgrawala dlm2 = RicartAgrawala(a2);
    RicartAgrawala dlm3 = RicartAgrawala(a3);

    // Define critical resource
    std::string rsc1 = "rsc1";
    
    // dlm1 should not be interested in the resource.
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::NOT_INTERESTED);

    // Let dlm1 lock rsc1
    dlm1.lock(rsc1, boost::assign::list_of(a2)(a3));
    // He should be INTERESTED now
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::INTERESTED);
    
    // OutgoingMessages should contain one messages (2 receivers)
    BOOST_CHECK(dlm1.hasOutgoingMessages());
    ACLMessage dlm1msg = dlm1.popNextOutgoingMessage();
    BOOST_CHECK(!dlm1.hasOutgoingMessages());
    BOOST_REQUIRE_EQUAL(2, dlm1msg.getAllReceivers().size());

    // Lets foward this mail to our agents
    dlm2.onIncomingMessage(dlm1msg);
    dlm3.onIncomingMessage(dlm1msg);

    // They should now have responded to agent1
    BOOST_CHECK(dlm2.hasOutgoingMessages());
    BOOST_CHECK(dlm3.hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2.popNextOutgoingMessage();
    ACLMessage dlm3msg = dlm3.popNextOutgoingMessage();
    BOOST_CHECK(!dlm2.hasOutgoingMessages());
    BOOST_CHECK(!dlm3.hasOutgoingMessages());

    // Lets forward the responses
    dlm1.onIncomingMessage(dlm2msg);
    dlm1.onIncomingMessage(dlm3msg);

    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::LOCKED);

    // We release the lock and check it has been released
    dlm1.unlock(rsc1);
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::NOT_INTERESTED);
}

/**
 * Two agents want the same resource. First, a2 wants it _later_ than a1, then a1 wants it while
 * a2 holds the lock.
 */
BOOST_AUTO_TEST_CASE(two_agents_conflict)
{
    // Create 2 Agents
    Agent a1 ("agent1"), a2 ("agent2");
    // Create 3 DLMs
    RicartAgrawala dlm1 = RicartAgrawala(a1);
    RicartAgrawala dlm2 = RicartAgrawala(a2);

    // Define critical resource
    std::string rsc1 = "rsc1";
    
    // Let dlm1 lock rsc1
    dlm1.lock(rsc1, boost::assign::list_of(a2));
    // OutgoingMessages should contain one messages
    BOOST_CHECK(dlm1.hasOutgoingMessages());
    ACLMessage dlm1msg = dlm1.popNextOutgoingMessage();
    BOOST_CHECK(!dlm1.hasOutgoingMessages());
    
    // Now (later) a2 tries to lock the same resource
    dlm2.lock(rsc1, boost::assign::list_of(a1));
    // OutgoingMessages should contain one messages
    BOOST_CHECK(dlm2.hasOutgoingMessages());
    ACLMessage dlm2msg = dlm2.popNextOutgoingMessage();
    BOOST_CHECK(!dlm2.hasOutgoingMessages());

    // Lets foward both mails
    dlm2.onIncomingMessage(dlm1msg);
    dlm1.onIncomingMessage(dlm2msg);
    
    // Only a2 should have responded by now
    BOOST_CHECK(dlm2.hasOutgoingMessages());
    BOOST_CHECK(!dlm1.hasOutgoingMessages());
    ACLMessage dlm2rsp = dlm2.popNextOutgoingMessage();
    BOOST_CHECK(!dlm2.hasOutgoingMessages());

    // Lets forward the response
    dlm1.onIncomingMessage(dlm2rsp);
    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::LOCKED);
    // We release the lock and check it has been released
    dlm1.unlock(rsc1);
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::NOT_INTERESTED);
    // Now agent1 should have a new outgoing message, than has been deferred earlier
    BOOST_CHECK(dlm1.hasOutgoingMessages());
    ACLMessage dlm1rsp = dlm1.popNextOutgoingMessage();
    BOOST_CHECK(!dlm1.hasOutgoingMessages());
    
    // We forward it
    dlm2.onIncomingMessage(dlm1rsp);
    // Now, agent2 should hold the lock
    BOOST_CHECK(dlm2.getLockState(rsc1) == DLM::LOCKED);
    
    // a1 requests the same resource again
    dlm1.lock(rsc1, boost::assign::list_of(a2));
    // Let us forward his message directly
    dlm2.onIncomingMessage(dlm1.popNextOutgoingMessage());
    // a2 should defer the response
    BOOST_CHECK(!dlm2.hasOutgoingMessages());
    
    // Now a2 releases the lock, and should have a new outgoing message, which we forward
    dlm2.unlock(rsc1);
    BOOST_CHECK(dlm2.getLockState(rsc1) == DLM::NOT_INTERESTED);
    dlm1.onIncomingMessage(dlm2.popNextOutgoingMessage());
    
    // Finally a1 holds the lock again, and releases it again.
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::LOCKED);
    dlm1.unlock(rsc1);
    BOOST_CHECK(dlm1.getLockState(rsc1) == DLM::NOT_INTERESTED);
    // Everyone is happy
}

// TODO test for same time