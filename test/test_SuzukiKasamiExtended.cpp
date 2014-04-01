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
BOOST_AUTO_TEST_CASE(suzuki_kasami_extended_non_responding_agent)
{
    std::cout << "suzuki_kasami_extended_non_responding_agent" << std::endl;

    // Create 2 Agents
    Agent a1 ("agent1"), a2 ("agent2"), a3("agent3");
    // Define critical resource
    std::string rsc1 = "resource";
    // and a vector containing it
    std::vector<std::string> rscs;
    rscs.push_back(rsc1);

    // Create 3 DLMs
    DLM* dlm1 = DLM::dlmFactory(protocol::SUZUKI_KASAMI_EXTENDED, a1, rscs);
    DLM* dlm2 = DLM::dlmFactory(protocol::SUZUKI_KASAMI_EXTENDED, a2, std::vector<std::string>());
    DLM* dlm3 = DLM::dlmFactory(protocol::SUZUKI_KASAMI_EXTENDED, a3, std::vector<std::string>());

    // Agent 3 locks
    dlm3->lock(rsc1, boost::assign::list_of(a1)(a2));
    forwardAllMessages(boost::assign::list_of(dlm3)(dlm2)(dlm1));
    
    // Agent 2 tries to lock
    dlm2->lock(rsc1, boost::assign::list_of(a1));
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1)(dlm3));
    
    //
    // Now, agent 3 is disconnected from the other agents.
    //
    // He unlocks, but gets a failure message back, when he tries to send the token to a1.
    dlm3->unlock(rsc1);
    // No message forwarding
    // There can be multiple outgoing messages!
    while(dlm3->hasOutgoingMessages())
    {
        ACLMessage msgOut = dlm3->popNextOutgoingMessage();
        ACLMessage innerFailureMsg;
        innerFailureMsg.setPerformative(ACLMessage::INFORM);
        innerFailureMsg.setSender(AgentID(a3.identifier));
        innerFailureMsg.setAllReceivers(msgOut.getAllReceivers());
        innerFailureMsg.setContent("description: message delivery failed");
        ACLMessage outerFailureMsg;
        outerFailureMsg.setPerformative(ACLMessage::FAILURE);
        outerFailureMsg.setSender(AgentID("mts"));
        outerFailureMsg.addReceiver(AgentID(a3.identifier));
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
    
    
    // On the other side, both agent 1 and 2 should notice the failure somehow, and agent 1 should forward a new token to agent 2.
    dlm1->trigger();
    dlm2->trigger();
    // We sleep 2x 3s (1s more than the threshold) and call the trigger() method again.
    boost::this_thread::sleep_for(boost::chrono::seconds(3));
    // The sleeping is split, so that a2 does not think a1 died.
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    boost::this_thread::sleep_for(boost::chrono::seconds(3));
    dlm1->trigger();
    dlm2->trigger();
    
    forwardAllMessages(boost::assign::list_of(dlm2)(dlm1));
    
    // A2 should now have obtained the lock
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

    // Now, agent1 dies
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
