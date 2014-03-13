#include "DLM.hpp"

#include <stdexcept>

namespace fipa {
namespace distributed_locking {

DLM::DLM()
{
}

DLM::DLM(const Agent& self)
    : mSelf(self)
{
}

void DLM::setSelf(const Agent& self)
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

void DLM::lock(const std::string& resource, const std::list< Agent >& agents)
{
    throw std::runtime_error("DLM::lock not implemented");
}

void DLM::unlock(const std::string& resource)
{
    throw std::runtime_error("DLM::unlock not implemented");
}

lock_state::LockState DLM::getLockState(const std::string& resource)
{
    throw std::runtime_error("DLM::getLockState not implemented");
}

void DLM::onIncomingMessage(const acl::ACLMessage& message)
{
    throw std::runtime_error("DLM::onIncomingMessage not implemented");
}

} // namespace distributed_locking
} // namespace fipa