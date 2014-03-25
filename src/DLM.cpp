#include "DLM.hpp"
#include "RicartAgrawala.hpp"
#include "RicartAgrawalaExtended.hpp"
#include "SuzukiKasami.hpp"

#include <stdexcept>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

namespace fipa {
namespace distributed_locking {
    
// Initialize the Protocol->string mapping
std::map<protocol::Protocol, std::string> DLM::protocolTxt = boost::assign::map_list_of
    (protocol::RICART_AGRAWALA, "ricart_agrawala")
    (protocol::RICART_AGRAWALA_EXTENDED, "ricart_agrawala_extended")
    (protocol::SUZUKI_KASAMI, "suzuki_kasami");
// And our own protocol string
const std::string DLM::dlmProtocolStr = "dlm";
    
DLM* DLM::dlmFactory(fipa::distributed_locking::protocol::Protocol implementation, const fipa::Agent& self, const std::vector< std::string >& resources)
{
    switch(implementation)
    {
        case protocol::RICART_AGRAWALA:
            return new RicartAgrawala(self, resources);
        case protocol::RICART_AGRAWALA_EXTENDED:
            return new RicartAgrawalaExtended(self, resources);
        case protocol::SUZUKI_KASAMI:
            return new SuzukiKasami(self, resources);
        default:
            return NULL;
    }
}

DLM::DLM()
    : mConversationIDnum(0)
{
}

DLM::DLM(const Agent& self, const std::vector<std::string>& resources)
    : mSelf(self)
    , mConversationIDnum(0)
{
    for(unsigned int i = 0; i < resources.size(); i++)
    {
        mOwnedResources[resources[i]] = mSelf.identifier;
    }
}

std::string DLM::getProtocolTxt(protocol::Protocol protocol)
{
    return protocolTxt[protocol];
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
    // Debug:
    if(fipa::acl::ACLMessage::performativeFromString(message.getPerformative()) == fipa::acl::ACLMessage::FAILURE)
    {
        std::cout << message.toString() << std::endl;
    }
    
    // Check if it's the right protocol
    if(message.getProtocol() != dlmProtocolStr)
    {
        return;
    }
    using namespace fipa::acl;
    // Abort if we're not a receiver
    AgentIDList receivers = message.getAllReceivers();
    bool foundUs = false;
    for(unsigned int i = 0; i < receivers.size(); i++)
    {
        AgentID agentID = receivers[i];
        if(agentID.getName() == mSelf.identifier)
        {
            foundUs = true;
            break;
        }
    }
    if(!foundUs)
    {
        return;
    }
    
    if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::REQUEST)
    {
        std::string resource = message.getContent();
        // If we are the physical owner of that resource, we reply with that information. Otherwise, we ignore the message
        if(mOwnedResources[resource] == mSelf.identifier)
        {
            // By making the reply also a broadcast, we can save messages later, if other agents want to lock the same resource
            ACLMessage response;
            response.setPerformative(ACLMessage::INFORM);

            // Our informOwnership messages are in the format "'OWNER'\nRESOURCE_IDENTIFIER"
            response.setContent("OWNER\n" + resource);
            // Add sender and receivers
            response.setSender(AgentID(mSelf.identifier));
            
            AgentIDList receivers = message.getAllReceivers();
            // remove ourselves..
            for(AgentIDList::iterator it = receivers.begin(); it != receivers.end(); it++)
            {
                if(*it == mSelf.identifier)
                {
                    receivers.erase(it);
                    break;
                }
            }
            // ..and add sender
            receivers.push_back(message.getSender());
            response.setAllReceivers(receivers);
            
            // Set conversation ID
            response.setConversationID(message.getConversationID());
            // The DLM protocol is not in the Protocol enum, as it is not a DLM implementation!
            response.setProtocol(dlmProtocolStr);
            
            // Add to outgoing messages
            mOutgoingMessages.push_back(response);
        }
    }
    else if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::INFORM)
    {
        // Split by newline
        std::vector<std::string> strs;
        std::string s = message.getContent();
        boost::split(strs, s, boost::is_any_of("\n")); // XXX why do I need to put it in an extra string?

        if(strs.size() != 2)
        {
            throw std::runtime_error("DLM::onIncomingMessage ACLMessage content malformed");
        }
        
        // Action depends on LOCK/UNLOCK/OWNER
        if(strs[0] == "LOCK")
        {
            mLockHolders[strs[1]] = message.getSender().getName();
        }
        else if(strs[0] == "UNLOCK" && message.getSender().getName() == mLockHolders[strs[1]])
        {
            // Only erase if the sender was the logical owner, as messages can come in wrong order
            mLockHolders.erase(strs[1]);
        }
        else if(strs[0] == "OWNER")
        {
            mOwnedResources[strs[1]] = message.getSender().getName();
        }
    }
}

void DLM::lockRequested(const std::string& resource, const std::list< Agent >& agents)
{
    // if we already know the physical owner of that resource, we don't have to do anything
    if(mOwnedResources.count(resource) != 0 && mOwnedResources[resource] != "")
    {
        return;
    }
    
    // Otherwise se send a broadcast message to get that information
    using namespace fipa::acl;
    ACLMessage message;
    message.setPerformative(ACLMessage::REQUEST);

    // Our requestOwnerInformation messages are in the format "RESOURCE_IDENTIFIER"
    message.setContent(resource);
    // Add sender and receivers
    message.setSender(AgentID(mSelf.identifier));
    for(std::list<Agent>::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        message.addReceiver(AgentID(it->identifier));
    }
    // Set and increase conversation ID
    message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    // The DLM protocol is not in the Protocol enum, as it is not a DLM implementation!
    message.setProtocol(dlmProtocolStr);
    
    // Add to outgoing messages
    mOutgoingMessages.push_back(message);
}

void DLM::lockObtained(const std::string& resource)
{
    if(mOwnedResources[resource] == mSelf.identifier)
    {
        // If this is our own resource, we can simply set us as the logical owner
        mLockHolders[resource] = mSelf.identifier;
    }
    else
    {
        // Otherwise, if we know the physical owner, we have to send him an ACL message
        if(mOwnedResources.count(resource) == 0 || mOwnedResources[resource] == "")
        {
            std::cout << "DLM::lockObtained WARN: " << mSelf.identifier << " did not know the owner of " << resource << std::endl;
            return;
        }
        
        using namespace fipa::acl;
        ACLMessage message;
        message.setPerformative(ACLMessage::INFORM);

        // Our lockObtained messages are in the format "'LOCK'\nRESOURCE_IDENTIFIER"
        message.setContent("LOCK\n" + resource);
        // Add sender and receiver
        message.setSender(AgentID(mSelf.identifier));
        message.addReceiver(AgentID(mOwnedResources[resource]));
        // Set and increase conversation ID
        message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
        // The DLM protocol is not in the Protocol enum, as it is not a DLM implementation!
        message.setProtocol(dlmProtocolStr);
        
        // Add to outgoing messages
        mOutgoingMessages.push_back(message);
    }
}

void DLM::lockReleased(const std::string& resource)
{
    if(mOwnedResources[resource] == mSelf.identifier)
    {
        // If this is our own resource, we can simply unset us as the logical owner
        // Only erase if we were the logical owner
        if(mLockHolders[resource] == mSelf.identifier)
        {
            mLockHolders.erase(resource);
        }
    }
    else
    {
        // Otherwise, if we know the physical owner, we have to send him an ACL message
        if(mOwnedResources.count(resource) == 0 || mOwnedResources[resource] == "")
        {
            std::cout << "DLM::lockReleased WARN: " << mSelf.identifier << " did not know the owner of " << resource << std::endl;
            return;
        }
        
        using namespace fipa::acl;
        ACLMessage message;
        message.setPerformative(ACLMessage::INFORM);

        // Our lockReleased messages are in the format "'UNLOCK'\nRESOURCE_IDENTIFIER"
        message.setContent("UNLOCK\n" + resource);
        // Add sender and receiver
        message.setSender(AgentID(mSelf.identifier));
        message.addReceiver(AgentID(mOwnedResources[resource]));
        // Set and increase conversation ID
        message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
        // The DLM protocol is not in the Protocol enum, as it is not a DLM implementation!
        message.setProtocol(dlmProtocolStr);
        
        // Add to outgoing messages
        mOutgoingMessages.push_back(message);
    }
}

} // namespace distributed_locking
} // namespace fipa