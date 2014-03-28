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
// And our own protocol strings
const std::string DLM::dlmProtocolStr = "dlm";
const std::string DLM::probeProtocolStr = "probe";
// And other stuff
const int DLM::probeTimeoutSeconds = 5;

    
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

DLM::~DLM()
{
    // Nothing to do here.
}

std::string DLM::getProtocolTxt(protocol::Protocol protocol)
{
    return protocolTxt[protocol];
}

const Agent& DLM::getSelf() const
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

bool DLM::hasOutgoingMessages() const
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

lock_state::LockState DLM::getLockState(const std::string& resource) const
{
    throw std::runtime_error("DLM::getLockState not implemented");
}

void DLM::agentFailed(const std::string& agentName)
{
    throw std::runtime_error("DLM::agentFailed not implemented");
}

void DLM::onIncomingMessage(const acl::ACLMessage& message)
{
    // Debug:
    if(fipa::acl::ACLMessage::performativeFromString(message.getPerformative()) == fipa::acl::ACLMessage::FAILURE)
    {
        std::cout << message.toString() << std::endl;
    }
    
    // Check if it's the right protocol
    std::string protocol = message.getProtocol();
    if(protocol != dlmProtocolStr && protocol != probeProtocolStr)
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
    
    // Call either dlm or probe method
    if(protocol == dlmProtocolStr)
    {
        onIncomingDLMMessage(message);
    }
    else if(protocol == probeProtocolStr)
    {
        onIncomingProbeMessage(message);
    }
}

void DLM::onIncomingDLMMessage(const acl::ACLMessage& message)
{
    using namespace fipa::acl;
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
            // Start sending PROBE messages to the owner
            startRequestingProbes(message.getSender().getName(), strs[1]);
        }
        else if(strs[0] == "UNLOCK")
        {
            // Stop sending PROBE messages to the owner
            stopRequestingProbes(message.getSender().getName(), strs[1]);
            if(message.getSender().getName() == mLockHolders[strs[1]])
            {
                // Only erase if the sender was the logical owner, as messages can come in wrong order
                mLockHolders.erase(strs[1]);
            }
        }
        else if(strs[0] == "OWNER")
        {
            mOwnedResources[strs[1]] = message.getSender().getName();
        }
    }
}

void DLM::onIncomingProbeMessage(const acl::ACLMessage& message)
{
    using namespace fipa::acl;
    if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::REQUEST)
    {
        // Answer with Inform:SUCCESS
        ACLMessage response;
        response.setPerformative(ACLMessage::INFORM);
        response.setContent("SUCCESS");
        // Add sender and receiver
        response.setSender(AgentID(mSelf.identifier));
        response.addReceiver(message.getSender());
        // Set and increase conversation ID
        response.setConversationID(message.getConversationID());
        // The PROBE protocol is not in the Protocol enum, as it is not a DLM implementation!
        response.setProtocol(probeProtocolStr);
        // Add to outgoing messages
        mOutgoingMessages.push_back(response);
    }
    else if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::INFORM)
    {
        // Set success in the ProbeRunner
        mProbeRunners[message.getSender().getName()].mSuccess = true;
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

void DLM::startRequestingProbes(const std::string& agentName, const std::string resourceName)
{
    mProbeRunners[agentName].mResources.push_back(resourceName);
}

void DLM::stopRequestingProbes(const std::string& agentName, const std::string resourceName)
{
    if(mProbeRunners.count(agentName) != 0)
    {
        mProbeRunners[agentName].mResources.remove(resourceName);
        // The following optimizes a little, but is not actually necessary
        if(mProbeRunners[agentName].mResources.empty())
        {
            // delete this ProbeRunner
            mProbeRunners.erase(agentName);  
        }
    }
}

void DLM::trigger()
{
    // Loop through all ProbeRunners
    for(std::map<std::string, ProbeRunner>::iterator it = mProbeRunners.begin(); it != mProbeRunners.end(); it++)
    {
        // Only act, if the resource list is not empty
        if(!it->second.mResources.empty())
        {
            // If we already sent a probe message, and it is longer ago than the threshold,
            // we need to check for a response
            if(!it->second.mTimeStamp.isNull() && base::Time::now() > it->second.mTimeStamp + base::Time::fromSeconds(probeTimeoutSeconds))
            {
                if(it->second.mSuccess)
                {
                    // Send another probe and update timestamp
                    it->second.mTimeStamp = base::Time::now();
                    sendProbe(it->first);
                    std::cout << mSelf.identifier << " sent probe to " << it->first << " after getting a success response." << std::endl;
                }
                else
                {
                    // The agent failed. Stop annoying it then.
                    mProbeRunners.erase(it->first);
                    // And call agentFailed
                    agentFailed(it->first);
                    std::cout << mSelf.identifier << " got no response from " << it->first  << std::endl;
                }
            }
            // If we never sent a probe message, we better get going
            if(it->second.mTimeStamp.isNull())
            {
                it->second.mTimeStamp = base::Time::now();
                sendProbe(it->first);
                std::cout << mSelf.identifier << " sent probe to " << it->first << " for the first time." << std::endl;
            }
        }
    }
}

void DLM::sendProbe(const std::string& agentName)
{
    // When sending a probe, success is false until we get a response
    mProbeRunners[agentName].mSuccess = false;
    
    using namespace fipa::acl;
    ACLMessage message;
    message.setPerformative(ACLMessage::REQUEST);
    message.setContent("PROBE");
    // Add sender and receiver
    message.setSender(AgentID(mSelf.identifier));
    message.addReceiver(AgentID(agentName));
    // Set and increase conversation ID
    message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    // The PROBE protocol is not in the Protocol enum, as it is not a DLM implementation!
    message.setProtocol(probeProtocolStr);
    // Add to outgoing messages
    mOutgoingMessages.push_back(message);
}

} // namespace distributed_locking
} // namespace fipa