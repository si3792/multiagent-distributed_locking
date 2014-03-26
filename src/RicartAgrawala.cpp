#include "RicartAgrawala.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <stdexcept>

namespace fipa {
namespace distributed_locking {

// Set the protocol
const protocol::Protocol RicartAgrawala::protocol = protocol::RICART_AGRAWALA;
    
RicartAgrawala::RicartAgrawala() 
    : DLM()
{
}

RicartAgrawala::RicartAgrawala(const fipa::Agent& self, const std::vector< std::string >& resources) 
    : DLM(self, resources)
{
}

void RicartAgrawala::lock(const std::string& resource, const std::list<Agent>& agents)
{
    lock_state::LockState state = getLockState(resource);
    // Only act we are not holding this resource and not already interested in it
    if(state != lock_state::NOT_INTERESTED)
    {
        if(state == lock_state::UNREACHABLE)
        {
            // An unreachable resource cannot be locked. Throw exception
            throw std::runtime_error("RicartAgrawala::lock Cannot lock UNREACHABLE resource.");
        }
        return;
    }
    
    // Let the base class know we're requesting the lock BEFORE we actually do that
    lockRequested(resource, agents);

    using namespace fipa::acl;
    // Send a message to everyone, requesting the lock
    ACLMessage message;
    // A simple request
    message.setPerformative(ACLMessage::REQUEST);

    // Our request messages are in the format "TIME\nRESOURCE_IDENTIFIER"
    base::Time time = base::Time::now();
    message.setContent(time.toString() + "\n" + resource);
    // Add sender and receivers
    message.setSender(AgentID(mSelf.identifier));
    for(std::list<Agent>::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        message.addReceiver(AgentID(it->identifier));
    }
    // Set and increase conversation ID
    message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    message.setProtocol(protocolTxt[protocol]);

    // Add to outgoing messages
    mOutgoingMessages.push_back(message);
    
    // Change internal state
    mLockStates[resource].mCommunicationPartners = agents;
    // Sort agents
    mLockStates[resource].mCommunicationPartners.sort();
    
    mLockStates[resource].mResponded.clear();
    mLockStates[resource].mState = lock_state::INTERESTED;
    mLockStates[resource].mInterestTime = time;
    mLockStates[resource].mConversationID = message.getConversationID();

    // Now a response from each agent must be received before we can enter the critical section
}

void RicartAgrawala::unlock(const std::string& resource)
{
    // Only act we are actually holding this resource
    if(getLockState(resource) == lock_state::LOCKED)
    {
        // Change internal state
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
        
        // Let the base class know we released the lock
        lockReleased(resource);
    }
}

lock_state::LockState RicartAgrawala::getLockState(const std::string& resource)
{
    return mLockStates[resource].mState;
}

void RicartAgrawala::onIncomingMessage(const fipa::acl::ACLMessage& message)
{
    // Call base method as required
    DLM::onIncomingMessage(message);
    
    // Check if it's the right protocol
    if(message.getProtocol() != protocolTxt[protocol])
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
    
    // Check message type
    if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::REQUEST)
    {
        handleIncomingRequest(message);
    }
    else if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::INFORM)
    {
        handleIncomingResponse(message);
    }
    else if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::FAILURE)
    {
        handleIncomingFailure(message);
    }
    // We ignore other performatives, as they are not part of our protocol.
}

void RicartAgrawala::handleIncomingRequest(const fipa::acl::ACLMessage& message)
{
    base::Time otherTime;
    std::string resource;
    extractInformation(message, otherTime, resource);

    // Create a response
    fipa::acl::ACLMessage response;
    // An inform we're not interested in or done using the resource
    response.setPerformative(fipa::acl::ACLMessage::INFORM);

    // Add sender and receiver
    response.setSender(fipa::acl::AgentID(mSelf.identifier));
    response.addReceiver(message.getSender());

    // Keep the conversation ID
    response.setConversationID(message.getConversationID());
    response.setProtocol(protocolTxt[protocol]);
    
    // We send this message now, if we don't hold the resource and are not interested or have been slower. Otherwise we defer it.
    lock_state::LockState state = getLockState(resource);
    if(state == lock_state::NOT_INTERESTED || (state == lock_state::INTERESTED && otherTime < mLockStates[resource].mInterestTime))
    {
        // Our response messages are in the format "TIME\nRESOURCE_IDENTIFIER"
        response.setContent(base::Time::now().toString() +"\n" + resource);
        mOutgoingMessages.push_back(response);
    }
    else if(state == lock_state::INTERESTED && otherTime == mLockStates[resource].mInterestTime)
    {
        // If it should happen that 2 agents have the same timestamp, the interest is revoked, and they have to call lock() again
        // The following is nearly identical to unlock(), except that the prerequisite of being LOCKED is not met
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
    }
    else
    {
        // We will have to add the timestamp later!
        response.setContent(resource);
        mLockStates[resource].mDeferredMessages.push_back(response);
    }
}

void RicartAgrawala::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
    // If we get a response, that likely means, we are interested in a resource
    base::Time otherTime;
    std::string resource;
    extractInformation(message, otherTime, resource);

    // A response is only relevant if we're "INTERESTED"
    if(getLockState(resource) != lock_state::INTERESTED)
    {
        return;
    }
    
    // Save the sender 
    // XXX if agent becomes more complex, we need to copy it from mCommunicationPartners,
    // instead of creating a new one
    mLockStates[resource].mResponded.push_back(Agent (message.getSender().getName()));
    // Sort agents who responded
    mLockStates[resource].mResponded.sort();
    
    // We have got the lock, if all agents responded
    if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
    {
        mLockStates[resource].mState = lock_state::LOCKED;
        
        // Let the base class know we obtained the lock
        lockObtained(resource);
    }
}

void RicartAgrawala::handleIncomingFailure(const fipa::acl::ACLMessage& message)
{
    // First determine the affected resource from the conversation id.
    std::string conversationID = message.getConversationID();
    std::string resource;
    for(std::map<std::string, ResourceLockState>::const_iterator it = mLockStates.begin(); it != mLockStates.end(); it++)
    {
        if(it->second.mConversationID == conversationID)
        {
            resource = it->first;
            break;
        }
    }
    // Abort if we didn't find a corresponding resource, or are not interested in the resource currently
    if(resource == "" || mLockStates[resource].mState != lock_state::INTERESTED)
    {
        // If a response message cannot be delivered, we can ignore that
        return;
    }
    
    using namespace fipa::acl;
    // Get intended receivers
    std::string innerEncodedMsg = message.getContent();
    ACLMessage errorMsg;
    MessageParser::parseData(innerEncodedMsg, errorMsg, representation::STRING_REP);
    AgentIDList deliveryFailedForAgents = errorMsg.getAllReceivers();
    
    for(AgentIDList::const_iterator it = deliveryFailedForAgents.begin(); it != deliveryFailedForAgents.end(); it++)
    {
        // Now we must handle the failure appropriately
        handleIncomingFailure(resource, it->getName());
    }
}

void RicartAgrawala::handleIncomingFailure(const std::string& resource, std::string intendedReceiver)
{
    // If the physical owner of the resource failed, the ressource probably cannot be obtained any more.
    if(mOwnedResources[resource] == intendedReceiver)
    {
        // Mark resource as unreachable.
        mLockStates[resource].mState = lock_state::UNREACHABLE;
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
    }
    else
    {
        // The agent was not important, we just have to remove it from the list of communication partners, as we won't get a response from it
        mLockStates[resource].mCommunicationPartners.remove(Agent (intendedReceiver));
        
        // We have got the lock, if all agents responded
        if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
        {
            mLockStates[resource].mState = lock_state::LOCKED;
            
            // Let the base class know we obtained the lock
            lockObtained(resource);
        }
    }
}

void RicartAgrawala::agentFailed(const std::string& agentName)
{
    // Determine all resources, where we await an answer from that agent
    for(std::map<std::string, ResourceLockState>::const_iterator it = mLockStates.begin(); it != mLockStates.end(); it++)
    {
        // If we're interested and await an answer from that agent...
        if(it->second.mState == lock_state::INTERESTED &&
            std::find(it->second.mCommunicationPartners.begin(), it->second.mCommunicationPartners.end(), Agent(agentName)) != it->second.mCommunicationPartners.end() &&
            std::find(it->second.mResponded.begin(), it->second.mResponded.end(), Agent(agentName)) == it->second.mResponded.end())
        {
            handleIncomingFailure(it->first, agentName);
        }
        // If we're not interested or the agent already responded, we can ignore that
    }
}

void RicartAgrawala::extractInformation(const fipa::acl::ACLMessage& message, base::Time& time, std::string& resource)
{
    // Split by newline
    std::vector<std::string> strs;
    std::string s = message.getContent();
    boost::split(strs, s, boost::is_any_of("\n")); // XXX why do I need to put it in an extra string?

    if(strs.size() != 2)
    {
        throw std::runtime_error("RicartAgrawala::extractInformation ACLMessage content malformed");
    }
    // Save the extracted information in the references
    time = base::Time::fromString(strs[0]);
    resource = strs[1];
}

void RicartAgrawala::sendAllDeferredMessages(const std::string& resource)
{
    for(std::list<fipa::acl::ACLMessage>::iterator it = mLockStates[resource].mDeferredMessages.begin();
        it != mLockStates[resource].mDeferredMessages.end(); it++)
    {
        fipa::acl::ACLMessage msg = *it;
        // Include timestamp
        msg.setContent(base::Time::now().toString() +"\n" + msg.getContent());
        mOutgoingMessages.push_back(msg);
    }
    // Clear list
    mLockStates[resource].mDeferredMessages.clear();
}

} // namespace distributed_locking
} // namespace fipa