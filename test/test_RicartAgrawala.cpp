#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <distributed_locking/RicartAgrawala.hpp>

#include <iostream>

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

BOOST_AUTO_TEST_CASE(basic_hold_and_release)
{
//     // Create 3 Agents
//     Agent a1 ("agent1"), a2 ("agent2"), a3 ("agent3");
//     // Create 3 DLMs
//     RicartAgrawala dlm1 = RicartAgrawala(a1, boost::assign::list_of(a2)(a3));
//     RicartAgrawala dlm2 = RicartAgrawala(a2, boost::assign::list_of(a1)(a3));
//     RicartAgrawala dlm3 = RicartAgrawala(a3, boost::assign::list_of(a1)(a2));
// 
//     // Define critical resource
//     std::string rsc1 = "rsc1";
// 
//     // Let dlm1 lock rsc1
//     dlm1.lock(rsc1);
//     // OutgoingMessages should contain one messages (2 receivers)
//     std::vector<ACLMessage> dlm1out = dlm1.getOutgoingMessages();
//     BOOST_REQUIRE_EQUAL(1, dlm1out.size());
//     BOOST_REQUIRE_EQUAL(2, dlm1out[0].getAllReceivers().size());
// 
//     // Lets foward this mail to our agents
//     dlm2.onIncomingMessage(dlm1out[0]);
//     dlm3.onIncomingMessage(dlm1out[0]);
// 
//     // They should now have responded to agent1
//     std::vector<ACLMessage> dlm2out = dlm2.getOutgoingMessages();
//     std::vector<ACLMessage> dlm3out = dlm3.getOutgoingMessages();
//     BOOST_REQUIRE_EQUAL(1, dlm2out.size());
//     BOOST_REQUIRE_EQUAL(1, dlm3out.size());
// 
//     // Lets forward the responses
//     dlm1.onIncomingMessage(dlm2out[0]);
//     dlm1.onIncomingMessage(dlm3out[0]);
// 
//     // Now, agent1 should hold the lock
//     BOOST_CHECK(dlm1.isLocked(rsc1));
// 
//     // We release the lock and check it has been released
//     dlm1.unlock(rsc1);
//     BOOST_CHECK(!dlm1.isLocked(rsc1));
}