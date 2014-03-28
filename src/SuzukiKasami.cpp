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

SuzukiKasami::SuzukiKasami(const fipa::Agent& self, const std::vector< std::string >& resources) 
    : DLM(self, resources)
{
    // Also hold the token at the beginning for all physically owned resources
    for(unsigned int i = 0; i < resources.size(); i++)
    {
        mLockStates[resources[i]].mHoldingToken = true;
    }
}

void SuzukiKasami::lock(const std::string& resource, const std::list<Agent>& agents)
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
    
    // If we're holding the token, we can simply enter the critical section
    if(mLockStates[resource].mHoldingToken)
    {
        mLockStates[resource].mState = lock_state::LOCKED;
        // Let the base class know we obtained the lock
        lockObtained(resource);
        return;
    }
    
    // Let the base class know we're requesting the lock BEFORE we actually do that
    lockRequested(resource, agents);
    
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
    message.setConversationID(mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    message.setProtocol(protocolTxt[protocol]);
    
    // Add to outgoing messages
    mOutgoingMessages.push_back(message);
    
    // Change internal state (seq_no already changed)
    mLockStates[resource].mCommunicationPartners = agents;
    // Sort agents
    mLockStates[resource].mCommunicationPartners.sort();
    mLockStates[resource].mState = lock_state::INTERESTED;
    mLockStates[resource].mConversationID = message.getConversationID();

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
        forwardToken(resource);
        
        // Let the base class know we released the lock
        lockReleased(resource);
    }
}

void SuzukiKasami::forwardToken(const std::string& resource)
{
    // Forward token if there's a pending request (queue not empty)
    if(!mLockStates[resource].mToken.mQueue.empty())
    {
        std::string agentID = mLockStates[resource].mToken.mQueue.front();
        mLockStates[resource].mToken.mQueue.pop_front();
        sendToken(fipa::acl::AgentID (agentID), resource, mSelf.identifier + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    }
    // Else keep token
}

lock_state::LockState SuzukiKasami::getLockState(const std::string& resource) const
{
    if(mLockStates.count(resource) != 0)
    {
        return mLockStates.at(resource).mState;
    }
    else
    {
        // Otherwise return the default state
        return lock_state::NOT_INTERESTED;
    }
}

void SuzukiKasami::onIncomingMessage(const fipa::acl::ACLMessage& message)
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
    Token token;
    // This HAS to be done in two steps, as resource is not known before extractInformation return!
    extractInformation(message, resource, token);
    mLockStates[resource].mToken = token;
    
    // We're defenitely holding the token now, if we're interested or not
    mLockStates[resource].mHoldingToken = true;

    // Following, a response is only relevant if we're "INTERESTED"
    if(getLockState(resource) != lock_state::INTERESTED)
    {
        forwardToken(resource);
        return;
    }
    // Now we can lock the resource
    mLockStates[resource].mState = lock_state::LOCKED;
    
    // Let the base class know we obtained the lock
    lockObtained(resource);
}

void SuzukiKasami::handleIncomingFailure(const fipa::acl::ACLMessage& message)
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
        // TODO what do we do, if a response message cannot be delivered?
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

void SuzukiKasami::handleIncomingFailure(const std::string& resource, std::string intendedReceiver)
{
    bool wasTokenHolder = false;
    // If the physical owner of the resource failed, the ressource probably cannot be obtained any more.
    if(mOwnedResources[resource] == intendedReceiver)
    {
        // Mark resource as unreachable.
        mLockStates[resource].mState = lock_state::UNREACHABLE;
        // We cannot update the token, as we do not possess it, but this is probably no problem if the resource cannot be used any more
    }
    else if(wasTokenHolder)
    {
        // FIXME how to find out if he atually was the tokenholder?
        // TODO Everyone must revoke their interest, and lock newly.
        // The physical owner needs to create a new token and becomes the new tokenholder
    }
    else
    {
        // The agent was not important, we just have to remove it from the list of communication partners, as we won't get a response from it
        mLockStates[resource].mCommunicationPartners.remove(Agent (intendedReceiver));
    }
}

void SuzukiKasami::agentFailed(const std::string& agentName)
{
    // Determine all resources, where the agent is a communication partner
    for(std::map<std::string, ResourceLockState>::const_iterator it = mLockStates.begin(); it != mLockStates.end(); it++)
    {
        // If we're interested and communicated with that agent
        if(it->second.mState == lock_state::INTERESTED &&
            std::find(it->second.mCommunicationPartners.begin(), it->second.mCommunicationPartners.end(), Agent(agentName)) != it->second.mCommunicationPartners.end())
        {
            handleIncomingFailure(it->first, agentName);
        }
    }
    // If we're not interested, we can ignore that
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