#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <distributed_locking/DLM.hpp>

#include "TestHelper.hpp"

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

BOOST_AUTO_TEST_SUITE(ricart_agrawala)

/**
 * Test correct reactions if an agent fails, that is important.
 */
BOOST_AUTO_TEST_CASE(failing_of_important_agent)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/failing_of_important_agent");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 2 Agents
    AgentID a1 ("agent1"), a2 ("agent2");

    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 2 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());

    dlm2->discover(rsc1,  boost::assign::list_of(a1));
    dlm2->trigger();
    dlm1->trigger();
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    dlm2->trigger();
    dlm1->trigger();
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    dlm2->trigger();
    dlm1->trigger();
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // Let dlm2 lock and unlock rsc1 once, so that he knows dlm1 is the owner.
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // Now we simulate a failure of dlm1
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    // No message forwarding
    // There can be multiple outgoing messages!
    while(dlm2->hasOutgoingMessages())
    {
        ACLMessage msgOut = dlm2->popNextOutgoingMessage();
        ACLMessage innerFailureMsg;
        innerFailureMsg.setPerformative(ACLMessage::INFORM);
        innerFailureMsg.setSender(a2);
        innerFailureMsg.setAllReceivers(msgOut.getAllReceivers());
        innerFailureMsg.setContent("description: message delivery failed");

        ACLMessage outerFailureMsg;
        outerFailureMsg.setPerformative(ACLMessage::FAILURE);
        outerFailureMsg.setSender(AgentID("mts"));
        outerFailureMsg.addReceiver(a2);
        outerFailureMsg.setOntology("fipa-agent-management");
        outerFailureMsg.setProtocol(msgOut.getProtocol());
        outerFailureMsg.setConversationID(msgOut.getConversationID());
        outerFailureMsg.setContent(innerFailureMsg.toString());

        dlm2->onIncomingMessage(outerFailureMsg);
    }

    // a1 was owner of rsc1, so he should be considered important.
    // therefore, a2 should mark the resource as unobtainable
    BOOST_CHECK_MESSAGE(dlm2->getLockState(rsc1) == lock_state::UNREACHABLE, "Expected " << lock_state::UNREACHABLE << " vs. " << dlm2->getLockState(rsc1) );

    // Calling lock now should trigger an exception
    BOOST_CHECK_THROW(dlm2->lock(rsc1, boost::assign::list_of(a1)), std::runtime_error);
}

/**
 * Test correct reactions if an agent fails, that is not important.
 */
BOOST_AUTO_TEST_CASE(failing_agent_not_important)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/failing_agent_not_important");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 2 Agents
    AgentID a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create only a dlm for a2 (a1 is "dead")
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA, a2, rscs);

    // dlm2 owns rsc1 and therefore knows he's the owner

    // Now we simulate a failure of dlm1
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    BOOST_CHECK(dlm2->getLockState(rsc1) != lock_state::LOCKED);

    // No message forwarding!
    // There can be multiple outgoing messages!
    while(dlm2->hasOutgoingMessages())
    {
        ACLMessage msgOut = dlm2->popNextOutgoingMessage();
        ACLMessage innerFailureMsg;
        innerFailureMsg.setPerformative(ACLMessage::INFORM);
        innerFailureMsg.setSender(AgentID(a2));
        innerFailureMsg.addReceiver(a1);
        innerFailureMsg.setContent("description: message delivery failed");
        ACLMessage outerFailureMsg;
        outerFailureMsg.setPerformative(ACLMessage::FAILURE);
        outerFailureMsg.setSender(AgentID("mts"));
        outerFailureMsg.addReceiver(a2);
        outerFailureMsg.setOntology("fipa-agent-management");
        outerFailureMsg.setProtocol(msgOut.getProtocol());
        outerFailureMsg.setConversationID(msgOut.getConversationID());
        outerFailureMsg.setContent(innerFailureMsg.toString());

        dlm2->onIncomingMessage(outerFailureMsg);
    }

    // a1 was not owner of rsc1, so he should be considered not important.
    // therefore, a2 should hold the lock now
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);
}

/**
 * Test taken from the ruby script.
 */
BOOST_AUTO_TEST_CASE(test_from_ruby_script)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/test_from_ruby_script");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 3 Agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLM::Ptrs
    // a1 owns the resource
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());
    DLM::Ptr dlm3 = DLM::create(protocol::RICART_AGRAWALA, a3, std::vector<std::string>());

    dlm1->discover(rsc1,  boost::assign::list_of(a2)(a3));
    for(int i = 0; i < 3; ++i)
    {
        forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    }

    // Now we lock
    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // And check it is being locked
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    dlm2->discover(rsc1,  boost::assign::list_of(a1)(a3));
    for(int i = 0; i < 3; ++i)
    {
        forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    }

    // AgentID two tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1)(a3));
    // AgentID 1 release
    dlm1->unlock(rsc1);

    for(int i = 0; i < 3; ++i)
    {
        forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    }

    // Check it is being locked by a2 now
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);

    // A3 locks, sleep, A1 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    dlm1->lock(rsc1, boost::assign::list_of(a2)(a3));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

    // Unlock and see that he is NOT_INTERESTED
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));

    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Check it is being locked by a3 now
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::LOCKED);

    // AgentID 3 release
    dlm3->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));

    // Check it is being locked by a1 now
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);

    // AgentID 1 releasese
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));

 //   // Check no one is interested any more
 //   BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
 //   BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);
 //   BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}

/**
 * Simple test with 3 agents, where a1 requests, obtains, and releases the lock for a resource
 */
BOOST_AUTO_TEST_CASE(basic_hold_and_release)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/basic_hold_and_release");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 3 Agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());
    DLM::Ptr dlm3 = DLM::create(protocol::RICART_AGRAWALA, a3, std::vector<std::string>());

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
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2)(dlm3));
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
}

/**
 * Two agents want the same resource. First, a2 wants it _later_ than a1, then a1 wants it while
 * a2 holds the lock.
 */
BOOST_AUTO_TEST_CASE(two_agents_conflict)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/two_agents_conflict");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 2 Agents
    AgentID a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 2 DLM::Ptrs
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::RICART_AGRAWALA, a2, std::vector<std::string>());

    dlm1->discover(rsc1,  boost::assign::list_of(a2));
    dlm2->discover(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    dlm1->trigger();
    dlm2->trigger();

    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    dlm1->trigger();
    dlm2->trigger();

    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    dlm1->trigger();
    dlm2->trigger();

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));

    // Now (later) a2 tries to lock the same resource
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // Now, agent1 should hold the lock
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    // We release the lock and check it has been released
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    // Now agent1 should have a new outgoing message, that has been deferred earlier

    // Now, agent2 should hold the lock
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::LOCKED);

    // a1 requests the same resource again
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));

    // Now a2 releases the lock, and should have a new outgoing message, which we forward
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::NOT_INTERESTED);

    // Finally a1 holds the lock again, and releases it again.
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::LOCKED);
    dlm1->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::NOT_INTERESTED);
    // Everyone is happy
}


/**
 * Two agents (one simulated) want one resource at the same time. The agent should revoke his interest.
 */
BOOST_AUTO_TEST_CASE(same_time_conflict)
{
    BOOST_TEST_MESSAGE("ricart_agrawala/same_time_conflict");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create 2 Agents, a2 is simulated
    AgentID a1 ("agent1"), a2 ("agent2");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 1 DLM::Ptr
    DLM::Ptr dlm1 = DLM::create(protocol::RICART_AGRAWALA, a1, rscs);

    // Let dlm1 lock rsc1
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    // Now he should be interested
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);

    // Get the outgoing message
    ACLMessage dlm1msg = dlm1->popNextOutgoingMessage();

    // Simulated agent 2 has basically the same message. He "revoked" his interest after
    // "receiving" dlm1msg
    ACLMessage simMsg;
    simMsg.setPerformative(dlm1msg.getPerformative());
    simMsg.setContent(dlm1msg.getContent());
    simMsg.setSender(a2);
    simMsg.addReceiver(dlm1msg.getSender());
    simMsg.setConversationID(a2.getName() + "0");
    simMsg.setProtocol(dlm1msg.getProtocol());

    // Send this message to a1
    dlm1->onIncomingMessage(simMsg);
    // Now he should be still interested
    BOOST_CHECK(dlm1->getLockState(rsc1) == lock_state::INTERESTED);
}

BOOST_AUTO_TEST_SUITE_END()
