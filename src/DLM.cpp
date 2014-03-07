#include "DLM.hpp"

namespace fipa {
namespace distributed_locking {

DLM::DLM()
{
}

DLM::DLM(const Agent& self, const std::vector<Agent>& agents)
    : mSelf(self)
    , mAgents(agents)
{
}

void DLM::addAgent(Agent agent)
{
    mAgents.push_back(agent);
}

void DLM::setSelf(Agent self)
{
    mSelf = self;
}

fipa::acl::ACLMessage& DLM::popNextOutgoingMessage()
{
    // TODO return sth else if empty
    fipa::acl::ACLMessage& msg = mOutgoingMessages.front();
    mOutgoingMessages.pop_front();
    return msg;
}

} // namespace distributed_locking
} // namespace fipa