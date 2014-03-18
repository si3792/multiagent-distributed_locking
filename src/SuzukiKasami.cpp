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
    message.setConversationID(mSelf.identifier + boost::lexical_cast<std::string>(mConversationIDnum++));
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
        
        // Iterate through all RequestNumbers known to the agent
        for(std::map<std::string, int>::iterator it = mLockStates[resource].mRequestNumber.begin();
            it != mLockStates[resource].mRequestNumber.end(); it++)
        {
            // If the request number of another agent is higher than the same request number known to the token
            // that means he requested the token. If he is not already in the queue, we add it.
            if(it->second == mLockStates[resource].mToken.mLastRequestNumber[it->first] + 1
                && std::find(mLockStates[resource].mToken.mQueue.begin(), mLockStates[resource].mToken.mQueue.end(), it->first) 
                == mLockStates[resource].mToken.mQueue.end()
            )
            {
                mLockStates[resource].mToken.mQueue.push_back(it->first);
            }
        }
        // Forward token if there's a pending request (queue not empty)
        if(!mLockStates[resource].mToken.mQueue.empty())
        {
            std::string agentID = mLockStates[resource].mToken.mQueue.front();
            mLockStates[resource].mToken.mQueue.pop_front();
            sendToken(fipa::acl::AgentID (agentID), resource, mSelf.identifier + boost::lexical_cast<std::string>(mConversationIDnum++));
        }
        // Else keep token
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
    // Extract information
    std::string resource;
    int sequence_number;
    extractInformation(message, resource, sequence_number);
    fipa::acl::AgentID agentID = message.getSender();
    std::string agentName = agentID.getName();
    
    // Update our request number if necessary
    if(mLockStates[resource].mRequestNumber[agentName] < sequence_number)
    {
        mLockStates[resource].mRequestNumber[agentName] = sequence_number;
    }
    
    // If we hold the token && are not holding the lock && the sequence_number 
    // indicates an outstanding request: we send the token.
    if(mLockStates[resource].mHoldingToken && getLockState(resource) != lock_state::LOCKED
        && mLockStates[resource].mRequestNumber[agentName] == mLockStates[resource].mToken.mLastRequestNumber[agentName] + 1)
    {
        sendToken(agentID, resource, message.getConversationID());
    }
}

void SuzukiKasami::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
    // If we get a response, that likely means, we are interested in a resource
    // Extract information
    std::string resource;
    extractInformation(message, resource, mLockStates[resource].mToken);
    // We're defenitely holding the token now, if we're interested or not
    mLockStates[resource].mHoldingToken = true;

    // Following, a response is only relevant if we're "INTERESTED"
    if(getLockState(resource) != lock_state::INTERESTED)
    {
        return;
    }
    // Now we can lock the resource
    mLockStates[resource].mState = lock_state::LOCKED;
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
    // Restore the token
    // create and open an archive for input
    std::stringstream ss(message.getContent());
    boost::archive::text_iarchive ia(ss);
    // read state from archive
    ia >> resource;
    ia >> token;
    // archive and stream closed when destructors are called
}

void SuzukiKasami::sendToken(const fipa::acl::AgentID& receiver, const std::string& resource, const std::string& conversationID)
{
    // We must unset holdingToken
    mLockStates[resource].mHoldingToken = false;
    
    using namespace fipa::acl;
    ACLMessage tokenMessage;
    // Token messages use INFORM as performative
    tokenMessage.setPerformative(ACLMessage::INFORM);

    // Add sender and receiver
    tokenMessage.setSender(AgentID(mSelf.identifier));
    tokenMessage.addReceiver(receiver);

    tokenMessage.setConversationID(conversationID);
    tokenMessage.setProtocol(protocolTxt[protocol]);
    
    // Our response messages are in the format "BOOST_ARCHIVE(RESOURCE_IDENTIFIER, TOKEN)"
    std::stringstream ss;
    // save data to archive
    boost::archive::text_oarchive oa(ss);
    // write class instance to archive
    oa << resource;
    oa << mLockStates[resource].mToken;
    tokenMessage.setContent(ss.str());
    
    mOutgoingMessages.push_back(tokenMessage);
}

} // namespace distributed_locking
} // namespace fipa