#include "SuzukiKasami.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/deque.hpp>
#include <string>
#include <sstream>
#include <stdexcept>

namespace fipa {
namespace distributed_locking {

// Set the protocol
const protocol::Protocol SuzukiKasami::protocol = protocol::SUZUKI_KASAMI;
    
SuzukiKasami::SuzukiKasami() 
    : DLM()
{
}

SuzukiKasami::SuzukiKasami(const Agent& self) 
    : DLM(self)
{
}

void SuzukiKasami::lock(const std::string& resource, const std::list<Agent>& agents)
{
    // Only act we are not holding this resource and not already interested in it
    if(getLockState(resource) != lock_state::NOT_INTERESTED)
    {
        return;
    }
    
    // If we're holding the token, we can simply enter the critical section
    if(mLockStates[resource].mHoldingToken)
    {
        mLockStates[resource].mState = lock_state::LOCKED;
        return;
    }
    
    // Increase our sequence_number
    mLockStates[resource].mRequestNumber[mSelf.identifier]++;

    using namespace fipa::acl;
    // Send a message to everyone, requesting the lock
    ACLMessage message;
    // A simple request
    message.setPerformative(ACLMessage::REQUEST);

    // Our request messages are in the format "RESOURCE_IDENTIFIER\nSEQUENCE_NUMBER"
    message.setContent(resource + "\n" + boost::lexical_cast<std::string>(mLockStates[resource].mRequestNumber[mSelf.identifier]));
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
    
    // Change internal state (seq_no already changed)
    mLockStates[resource].mCommunicationPartners = agents;
    // Sort agents
    mLockStates[resource].mCommunicationPartners.sort();
    mLockStates[resource].mState = lock_state::INTERESTED;

    // Now the token must be obtained before we can enter the critical section
}

void SuzukiKasami::unlock(const std::string& resource)
{
    // Only act we are actually holding this resource
    if(getLockState(resource) == lock_state::LOCKED)
    {
        // Change internal state
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;
        
        // Update ID to have been executed
        mLockStates[resource].mToken.mLastRequestNumber[mSelf.identifier] = mLockStates[resource].mRequestNumber[mSelf.identifier];
        // TODO
//         // Add missing agents to the token Q
//         foreach(string agentID not in token.Q)
//         {
//             if(RN[agentID] = LN[agentID] + 1)
//             {
//             token.Q.push_back(agentID);
//             }
//         }
//         // Forward token if there's a pending request
//         if(!token.Q.isEmpty())
//         {
//             string agentID = token.Q.pop_front();
//             sendTokenTo(agentID);
//         }
//         // Else keep token
    }
}

lock_state::LockState SuzukiKasami::getLockState(const std::string& resource)
{
    return mLockStates[resource].mState;
}

void SuzukiKasami::onIncomingMessage(const fipa::acl::ACLMessage& message)
{
    using namespace fipa::acl;
    // Check if it's the right protocol
    if(message.getProtocol() != protocolTxt[protocol])
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

void SuzukiKasami::handleIncomingRequest(const fipa::acl::ACLMessage& message)
{
//     base::Time otherTime;
//     std::string resource;
//     extractInformation(message, otherTime, resource);
// 
//     // Create a response
//     fipa::acl::ACLMessage response;
//     // An inform we're not interested in or done using the resource
//     response.setPerformative(fipa::acl::ACLMessage::INFORM);
// 
//     // Add sender and receiver
//     response.setSender(fipa::acl::AgentID(mSelf.identifier));
//     response.addReceiver(message.getSender());
// 
//     // Keep the conversation ID
//     response.setConversationID(message.getConversationID());
//     response.setProtocol(protocolTxt[protocol]);
//     
//     // We send this message now, if we don't hold the resource and are not interested or have been slower. Otherwise we defer it.
//     lock_state::LockState state = getLockState(resource);
//     if(state == lock_state::NOT_INTERESTED || (state == lock_state::INTERESTED && otherTime < mLockStates[resource].mInterestTime))
//     {
//         // Our response messages are in the format "TIME\nRESOURCE_IDENTIFIER"
//         response.setContent(base::Time::now().toString() +"\n" + resource);
//         mOutgoingMessages.push_back(response);
//     }
//     else if(state == lock_state::INTERESTED && otherTime == mLockStates[resource].mInterestTime)
//     {
//         // If it should happen that 2 agents have the same timestamp, the interest is revoked, and they have to call lock() again
//         // The following is identical to unlock(), except that the prerequisite of being LOCKED is not met
//         mLockStates[resource].mState = lock_state::NOT_INTERESTED;
//         // Send all deferred messages for that resource
//         sendAllDeferredMessages(resource);
//     }
//     else
//     {
//         // We will have to add the timestamp later!
//         response.setContent(resource);
//         mLockStates[resource].mDeferredMessages.push_back(response);
//     }
}

void SuzukiKasami::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
//     // If we get a response, that likely means, we are interested in a resource
//     base::Time otherTime;
//     std::string resource;
//     extractInformation(message, otherTime, resource);
// 
//     // A response is only relevant if we're "INTERESTED"
//     if(getLockState(resource) != lock_state::INTERESTED)
//     {
//         return;
//     }
//     
//     // Save the sender
//     mLockStates[resource].mResponded.push_back(Agent (message.getSender().getName()));
//     // Sort agents who responded
//     mLockStates[resource].mResponded.sort();
//     // We have got the lock, if all agents responded
//     if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
//     {
//         mLockStates[resource].mState = lock_state::LOCKED;
//     }
}

void SuzukiKasami::extractInformation(const acl::ACLMessage& message, std::string& resource, int& sequence_number)
{
    // Split by newline
    std::vector<std::string> strs;
    std::string s = message.getContent();
    boost::split(strs, s, boost::is_any_of("\n"));

    if(strs.size() != 2)
    {
        throw std::runtime_error("SuzukiKasami::extractInformation ACLMessage content malformed");
    }
    // Save the extracted information in the references
    resource = strs[0];
    sequence_number = boost::lexical_cast<int>(strs[1]);
}

void SuzukiKasami::extractInformation(const acl::ACLMessage& message, std::string& resource, SuzukiKasami::Token& token)
{
    // Split by newline
    std::vector<std::string> strs;
    std::string s = message.getContent();
    boost::split(strs, s, boost::is_any_of("\n"));

    if(strs.size() != 2)
    {
        throw std::runtime_error("SuzukiKasami::extractInformation ACLMessage content malformed");
    }
    // Save the extracted information in the references
    resource = strs[0];
    
    // FIXME -lboost_serialization missing?
    
    // Restore the token
    // create and open an archive for input
    std::stringstream ss(strs[1]);
    //boost::archive::text_iarchive ia(ss);
    // read state from archive
    //ia >> token;
    // archive and stream closed when destructors are called
}

void SuzukiKasami::sendToken(const std::string& receiver, const std::string& resource)
{

}

} // namespace distributed_locking
} // namespace fipa