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

RicartAgrawala::RicartAgrawala(const Agent& self) 
    : DLM(self)
{
}

void RicartAgrawala::lock(const std::string& resource, const std::list<Agent>& agents)
{
    // Only act we are not holding this resource and not already interested in it
    if(getLockState(resource) != lock_state::NOT_INTERESTED)
    {
        return;
    }

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
    message.setConversationID(mSelf.identifier + boost::lexical_cast<std::string>(mConversationIDnum));
    mConversationIDnum++;
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
    }
}

lock_state::LockState RicartAgrawala::getLockState(const std::string& resource)
{
    return mLockStates[resource].mState;
}

void RicartAgrawala::onIncomingMessage(const fipa::acl::ACLMessage& message)
{    
    using namespace fipa::acl;
    // Check if it's the right protocol
    if(message.getProtocol() != protocolTxt[protocol])
    {
        return;
    }
    // Abort if we're not a receiver
    AgentIDList receivers = message.getAllReceivers();
    bool foundUs = false;
    for(int i = 0; i < receivers.size(); i++)
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
    
    std::cout << mSelf.identifier << " got request message for " << resource << " from " << message.getSender().getName() << ". ";
    
    // We send this message now, if we don't hold the resource and are not interested or have been slower. Otherwise we defer it.
    lock_state::LockState state = getLockState(resource);
    if(state == lock_state::NOT_INTERESTED || (state == lock_state::INTERESTED && otherTime < mLockStates[resource].mInterestTime))
    {
        std::cout << "Answering now." << std::endl;
        // Our response messages are in the format "TIME\nRESOURCE_IDENTIFIER"
        response.setContent(base::Time::now().toString() +"\n" + resource);
        mOutgoingMessages.push_back(response);
    }
    else if(state == lock_state::INTERESTED && otherTime == mLockStates[resource].mInterestTime)
    {
        std::cout << "SAME TIMESTAMP." << std::endl;
        // If it should happen that 2 agents have the same timestamp, the interest is revoked, and they have to call lock() again
        // The following is identical to unlock(), except that the prerequisite of being LOCKED is not met
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
    }
    else
    {
        std::cout << "Answering later." << std::endl;
        // We will have to add the timestamp later!
        response.setContent(resource);
        mLockStates[resource].mDeferredMessages.push_back(response);
    }
}

void RicartAgrawala::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
    std::cout << mSelf.identifier << " got response message." << std::endl;
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
    mLockStates[resource].mResponded.push_back(Agent (message.getSender().getName()));
    // Sort agents who responded
    mLockStates[resource].mResponded.sort();
    // FIXME
    std::cout << mSelf.identifier << " got responses for " << resource << " from: ";
    for(std::list<fipa::Agent>::const_iterator it = mLockStates[resource].mResponded.begin(); it != mLockStates[resource].mResponded.end(); it++)
    {
        std::cout << it->identifier << " ";
    }
    std::cout << std::endl;
    
    // We have got the lock, if all agents responded
    if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
    {
        mLockStates[resource].mState = lock_state::LOCKED;
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
        std::cout << mSelf.identifier << " send deferred response for " << resource << " to " << msg.getAllReceivers().front().getName() << std::endl;
        // Include timestamp
        msg.setContent(base::Time::now().toString() +"\n" + msg.getContent());
        mOutgoingMessages.push_back(msg);
    }
    // Clear list
    mLockStates[resource].mDeferredMessages.clear();
}

} // namespace distributed_locking
} // namespace fipa