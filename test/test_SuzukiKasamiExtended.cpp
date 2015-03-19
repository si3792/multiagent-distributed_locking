#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>
#include <distributed_locking/AgentIDSerialization.hpp>
#include <distributed_locking/DLM.hpp>
#include <distributed_locking/SuzukiKasamiExtended.hpp>
#include <sstream>

#include "TestHelper.hpp"

using namespace fipa;
using namespace fipa::acl;
using namespace fipa::distributed_locking;

BOOST_AUTO_TEST_SUITE(suzuki_kasami_extended)

BOOST_AUTO_TEST_CASE(agent_serialization)
{
    fipa::acl::AgentID agent0("test-agent-0");
    fipa::acl::AgentID agent1("test-agent-1");
    std::stringstream ss;
    {
        boost::archive::text_oarchive oa(ss);
        oa << agent0;
    }
    {
        AgentID agentOut;
        boost::archive::text_iarchive ia(ss);
        // read state from archive
        ia >> agentOut;
        BOOST_REQUIRE_MESSAGE(agentOut.getName() == agent0.getName(), "Agent serialization: " << agentOut.getName() << " vs. expected " << agent0.getName());
    }

}

BOOST_AUTO_TEST_CASE(token_serialization)
{
    fipa::acl::AgentID agent0("test-agent-0");
    fipa::acl::AgentID agent1("test-agent-1");

    std::string resourceIn = "test-resource";
    std::stringstream ss;
    SuzukiKasami::Token tokenIn;
    {
        tokenIn.mLastRequestNumber[agent0] = 1;
        tokenIn.mQueue.push_back(agent1);

        // Our response messages are in the format "BOOST_ARCHIVE(RESOURCE_IDENTIFIER, TOKEN)"
        boost::archive::text_oarchive oa(ss);
        // write class instance to archive
        oa << resourceIn;
        oa << tokenIn;
    }

    std::string resourceOut;
    SuzukiKasami::Token tokenOut;
    {
        boost::archive::text_iarchive ia(ss);
        // read state from archive
        ia >> resourceOut;
        ia >> tokenOut;
    }
    BOOST_REQUIRE_MESSAGE(resourceIn == resourceOut, "Resource: " << resourceIn << " vs. " << resourceOut);
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    BOOST_REQUIRE_MESSAGE(tokenOut.mLastRequestNumber[agent0] == 1, "Token out (#" << tokenOut.mLastRequestNumber.size() << ") : "<< agent0.getName() << ": " << tokenOut.mLastRequestNumber[agent0] << " vs. 1");
    BOOST_REQUIRE_MESSAGE(tokenOut.mQueue.size() == 1, "Token queue size 1");
    BOOST_REQUIRE_MESSAGE(tokenOut.mQueue.front() == agent1, "Token queue contains " << agent1.getName());
}

/**
 * Test correct reactions if an agent does not respond PROBE messages.
 */
BOOST_AUTO_TEST_CASE(non_responding_agent)
{
    BOOST_TEST_MESSAGE("suzuki_kasami_extended_non_responding_agent");
    fipa::acl::StateMachineFactory::setProtocolResourceDir( getProtocolPath() );

    // Create agents
    AgentID a1 ("agent1"), a2 ("agent2"), a3("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLM
    // A1 is resource owner
    DLM::Ptr dlm1 = DLM::create(protocol::SUZUKI_KASAMI_EXTENDED, a1, rscs);
    DLM::Ptr dlm2 = DLM::create(protocol::SUZUKI_KASAMI_EXTENDED, a2, std::vector<std::string>());
    DLM::Ptr dlm3 = DLM::create(protocol::SUZUKI_KASAMI_EXTENDED, a3, std::vector<std::string>());

    dlm3->discover(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    BOOST_REQUIRE(dlm3->hasKnownOwner(rsc1));

    // AgentID 3 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));

    BOOST_REQUIRE(dlm3->getLockState(rsc1) == lock_state::LOCKED);

    // AgentID 2 tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1)(a3));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));

    //
    // Now, agent 3 is disconnected from the other agents.
    //
    // He unlocks, but gets a failure message back, when he tries to send the token to a1
    // -- by default the token is returned to the owner of the resource
    BOOST_REQUIRE(dlm3->getLockState(rsc1) == lock_state::LOCKED);
    dlm3->unlock(rsc1);

    // No message forwarding
    // There can be multiple outgoing messages!
    while(dlm3->hasOutgoingMessages())
    {
        ACLMessage msgOut = dlm3->popNextOutgoingMessage();
        ACLMessage innerFailureMsg;
        innerFailureMsg.setPerformative(ACLMessage::INFORM);
        innerFailureMsg.setSender(a3);
        innerFailureMsg.setAllReceivers(msgOut.getAllReceivers());
        innerFailureMsg.setContent("description: message delivery failed");
        ACLMessage outerFailureMsg;
        outerFailureMsg.setPerformative(ACLMessage::FAILURE);
        outerFailureMsg.setSender(AgentID("mts"));
        outerFailureMsg.addReceiver(a3);
        outerFailureMsg.setOntology("fipa-agent-management");
        outerFailureMsg.setProtocol(msgOut.getProtocol());
        outerFailureMsg.setConversationID(msgOut.getConversationID());
        outerFailureMsg.setContent(innerFailureMsg.toString());

        dlm3->onIncomingMessage(outerFailureMsg);
    }
    // This means, agent 3 should mark the resource as UNREACHABLE
    BOOST_CHECK(dlm3->getLockState(rsc1) == lock_state::UNREACHABLE);

    // Calling lock now should trigger an exception
    BOOST_CHECK_THROW(dlm3->lock(rsc1, boost::assign::list_of(a1)), std::runtime_error);

    // On the other side, both agent 1 and 2 should notice the failure,
    // and agent 1 should reclaim the token by default
    // Subsequently it will forward a new token to agent 2.
    // We have to sleep 2x3s (1s more than the threshold) and call the trigger() method again.
    for(int i = 0; i < 10; ++i)
    {
        BOOST_TEST_MESSAGE("Trigger");
        dlm1->trigger();
        dlm2->trigger();
        forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
        sleep(1);
    }

    // A2 should now have obtained the lock
    BOOST_CHECK_MESSAGE(dlm2->getLockState(rsc1) == lock_state::LOCKED, "Expected " << lock_state::LOCKED << " but found " << dlm2->getLockState(rsc1));

    // He unlocks
    dlm2->unlock(rsc1);
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // A1 locks
    dlm1->lock(rsc1, boost::assign::list_of(a2));
    forwardAllMessages(boost::assign::list_of(dlm1)(dlm2));

    // A2 tries to lock again
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));

    // Now, agent1 dies
    for(int i = 0; i < 10; ++i)
    {
        BOOST_TEST_MESSAGE("Trigger");
        dlm2->trigger();
        forwardAllMessages(boost::assign::list_of(dlm2));
        sleep(1);
    }
    dlm2->trigger();

    // a1 was owner of rsc1, so he should be considered important.
    // therefore, a2 should mark the resource as unobtainable
    BOOST_CHECK(dlm2->getLockState(rsc1) == lock_state::UNREACHABLE);
    // Calling lock now should trigger an exception
    BOOST_CHECK_THROW(dlm2->lock(rsc1, boost::assign::list_of(a1)), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
