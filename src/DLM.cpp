#include "DLM.hpp"
#include "RicartAgrawala.hpp"
#include "SuzukiKasami.hpp"

#include <stdexcept>
#include <boost/assign/list_of.hpp>

namespace fipa {
namespace distributed_locking {
    
// Initialize the Protocol->string mapping
std::map<protocol::Protocol, std::string> DLM::protocolTxt = boost::assign::map_list_of
    (protocol::RICART_AGRAWALA, "ricart_agrawala")
    (protocol::SUZUKI_KASAMI, "suzuki_kasami");
    
DLM* DLM::dlmFactory(protocol::Protocol implementation, const Agent& self)
{
    switch(implementation)
    {
        case protocol::RICART_AGRAWALA:
            return new RicartAgrawala(self);
        case protocol::SUZUKI_KASAMI:
            return new SuzukiKasami(self);
        default:
            return NULL;
    }
}

DLM::DLM()
{
}

DLM::DLM(const Agent& self)
    : mSelf(self)
{
}

const Agent& DLM::getSelf()
{
    return mSelf;
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