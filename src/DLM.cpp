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

fipa::acl::ACLMessage DLM::popNextOutgoingMessage()
{
    if(!hasOutgoingMessages())
    {
        throw std::runtime_error("DLM::popNextOutgoingMessage no messages");
    }
    fipa::acl::ACLMessage msg = mOutgoingMessages.front();
    mOutgoingMessages.pop_front();
    return msg;
}

bool DLM::hasOutgoingMessages()
{
    return mOutgoingMessages.size() != 0;
}

} // namespace distributed_locking
} // namespace fipa