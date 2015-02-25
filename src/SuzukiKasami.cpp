#include "SuzukiKasami.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <sstream>
#include <stdexcept>
#include <deque>

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

SuzukiKasami::SuzukiKasami(const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
    : DLM(protocol::SUZUKI_KASAMI, self, resources)
{
    // Also hold the token at the beginning for all physically owned resources
    for(unsigned int i = 0; i < resources.size(); i++)
    {
        mLockStates[resources[i]].mHoldingToken = true;
    }
}

void SuzukiKasami::lock(const std::string& resource, const std::list<AgentID>& agents)
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
    mLockStates[resource].mRequestNumber[mSelf]++;

    using namespace fipa::acl;
    // Send a message to everyone, requesting the lock
    ACLMessage message = prepareMessage(ACLMessage::REQUEST, getProtocolName());
    // Our request messages are in the format "RESOURCE_IDENTIFIER\nSEQUENCE_NUMBER"
    message.setContent(resource + "\n" + boost::lexical_cast<std::string>(mLockStates[resource].mRequestNumber[mSelf]));
    // Add receivers
    for(std::list<AgentID>::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        message.addReceiver(*it);
    }
    // Add to outgoing messages
    mOutgoingMessages.push_back(message);

    // Change internal state (seq_no already changed)
    mLockStates[resource].mCommunicationPartners = agents;
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
        mLockStates[resource].mToken.mLastRequestNumber[mSelf] = mLockStates[resource].mRequestNumber[mSelf];

        // Forward the token
        forwardToken(resource);

        // Let the base class know we released the lock
        lockReleased(resource);
    }
}

void SuzukiKasami::forwardToken(const std::string& resource)
{
    // Iterate through all RequestNumbers known to the agent
    std::map<AgentID, int>::iterator it = mLockStates[resource].mRequestNumber.begin();
    for(; it != mLockStates[resource].mRequestNumber.end(); ++it)
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
        AgentID agent = mLockStates[resource].mToken.mQueue.front();
        mLockStates[resource].mToken.mQueue.pop_front();
        sendToken(agent, resource, mSelf.getName() + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
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

bool SuzukiKasami::onIncomingMessage(const fipa::acl::ACLMessage& message)
{
    // Call base method as required
    if(DLM::onIncomingMessage(message))
    {
        return true;
    }

    // Check if it's the right protocol
    if(message.getProtocol() != getProtocolName())
    {
        return false;
    }

    using namespace fipa::acl;
    // Check message type
    switch(message.getPerformativeAsEnum())
    {
        case ACLMessage::REQUEST:
            handleIncomingRequest(message);
            return true;
        case ACLMessage::INFORM:
            handleIncomingResponse(message);
            return true;
        case ACLMessage::FAILURE:
            handleIncomingFailure(message);
            return true;
        default:
            return false;
    }
}

void SuzukiKasami::handleIncomingRequest(const fipa::acl::ACLMessage& message)
{
    // Extract information
    std::string resource;
    int sequence_number;
    extractInformation(message, resource, sequence_number);
    fipa::acl::AgentID agent = message.getSender();

    // Update our request number if necessary
    if(mLockStates[resource].mRequestNumber[agent] < sequence_number)
    {
        mLockStates[resource].mRequestNumber[agent] = sequence_number;
    }

    // If we hold the token && are not holding the lock && the sequence_number
    // indicates an outstanding request: we send the token.
    if(mLockStates[resource].mHoldingToken && getLockState(resource) != lock_state::LOCKED
        && mLockStates[resource].mRequestNumber[agent] == mLockStates[resource].mToken.mLastRequestNumber[agent] + 1)
    {
        sendToken(agent, resource, message.getConversationID());
    }
}

void SuzukiKasami::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
    // If we get a response, that likely means, we are interested in a resource
    // Extract information
    std::string resource;
    Token token;
    // This HAS to be done in two steps, as resource is not known before extractInformation returns!
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

    using namespace fipa::acl;
    // Get intended receivers
    std::string innerEncodedMsg = message.getContent();
    ACLMessage errorMsg;
    MessageParser::parseData(innerEncodedMsg, errorMsg, representation::STRING_REP);
    AgentIDList deliveryFailedForAgents = errorMsg.getAllReceivers();

    for(AgentIDList::const_iterator it = deliveryFailedForAgents.begin(); it != deliveryFailedForAgents.end(); it++)
    {
        if(resource == "")
        {
            // This means the message we tried to send was a token/response message.
            // We also have to deal with the failed agent
            agentFailed(it->getName());
            // In the non-extended algorithm, we lost the token and will probably starve now.
        }
        else
        {
            // Now we must handle the failure appropriately
            handleIncomingFailure(resource, it->getName());
        }
    }
}

void SuzukiKasami::handleIncomingFailure(const std::string& resource, const AgentID& intendedReceiver)
{
    // If the physical owner of the resource failed, the ressource probably cannot be obtained any more.
    if(mOwnedResources[resource] == intendedReceiver)
    {
        // Mark resource as unreachable.
        mLockStates[resource].mState = lock_state::UNREACHABLE;
        // We cannot update the token, as we do not possess it, but this is probably no problem if the resource cannot be used any more
        mLockStates[resource].mHoldingToken = false; // Just to be sure!
    }
    // This block cannot be triggered if only mLockHolders[resource] == intendedReceiver, as this can be erroneous
    else if(mOwnedResources[resource] == mSelf && isTokenHolder(resource, intendedReceiver))
    {
        // If we own the resource, we "get" the token again. We rediscover lost queue values by checking against our known request numbers (done in forwardToken)
        mLockStates[resource].mHoldingToken = true;
        if(getLockState(resource) != lock_state::INTERESTED)
        {
            // If we're not interested, we forward the token
            forwardToken(resource);
        }
        else
        {
            // Otherwise we can lock the resource
            mLockStates[resource].mState = lock_state::LOCKED;
            // Let the base class know we obtained the lock
            lockObtained(resource);
        }
        // Otherwise somebody will foward the token to us at some point. (else block)
    }
    else
    {
        // The agent was not important (for us), we have to remove it from the list of communication partners, as we won't get a response from it
        mLockStates[resource].mCommunicationPartners.remove(intendedReceiver);
        // We also have to remove his requestNumber(s) and remove him from the queue (relevant if we own the token)
        mLockStates[resource].mRequestNumber.erase(intendedReceiver);
        mLockStates[resource].mToken.mLastRequestNumber.erase(intendedReceiver);
        mLockStates[resource].mToken.mQueue.erase(std::remove(mLockStates[resource].mToken.mQueue.begin(), mLockStates[resource].mToken.mQueue.end(), intendedReceiver),
                                                  mLockStates[resource].mToken.mQueue.end());
    }
}

bool SuzukiKasami::isTokenHolder(const std::string& resource, const AgentID& agent)
{
    // Only the extension can return a representative value
    return false;
}

void SuzukiKasami::agentFailed(const AgentID& agent)
{
    // Here,we have to deal with the failure for all resources, as we don't know in which he was involved
    for(std::map<std::string, ResourceLockState>::const_iterator it = mLockStates.begin(); it != mLockStates.end(); it++)
    {
        handleIncomingFailure(it->first, agent);
    }
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
    ACLMessage tokenMessage = prepareMessage(ACLMessage::INFORM, getProtocolName());
    tokenMessage.addReceiver(receiver);
    tokenMessage.setConversationID(conversationID);

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
