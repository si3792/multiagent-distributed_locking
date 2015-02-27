#include "SuzukiKasami.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <sstream>
#include <stdexcept>
#include <deque>
#include <base/Logging.hpp>

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

void SuzukiKasami::lock(const std::string& resource, const AgentIDList& agents)
{
    if(!hasKnownOwner(resource))
    {
        throw std::invalid_argument("SuzukiKasami: cannot lock resource '" + resource + "' -- owner is unknown. Perform discovery first");
    }

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
        return;
    }

    requestToken(resource, agents);
}

void SuzukiKasami::requestToken(const std::string& resource, const AgentIDList& agents)
{
    // Increase sequence number
    int requestNumber = mLockStates[resource].mRequestNumber[mSelf] + 1;

    // Request token
    using namespace fipa::acl;
    ACLMessage message = prepareMessage(ACLMessage::REQUEST, getProtocolName());
    // TODO: Content Language
    // Our request messages are in the format "RESOURCE_IDENTIFIER\nSEQUENCE_NUMBER"
    message.setContent(resource + "\n" + boost::lexical_cast<std::string>(requestNumber));
    // Add receivers
    for(AgentIDList::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        message.addReceiver(*it);
    }
    // Add to outgoing messages
    sendMessage(message);

    // Change internal state (seq_no already changed)
    mLockStates[resource].mRequestNumber[mSelf] = requestNumber;
    mLockStates[resource].mCommunicationPartners = agents;
    mLockStates[resource].mState = lock_state::INTERESTED;
    mLockStates[resource].mConversationID[mSelf] = message.getConversationID();
    // Now the token must be obtained before we can enter the critical section
    LOG_DEBUG_S << "'" << mSelf.getName() << "' Token requested for resource '" << resource << "' sequence number: " << mLockStates[resource].mRequestNumber[mSelf];
}

void SuzukiKasami::unlock(const std::string& resource)
{
    LOG_DEBUG_S << "'" << mSelf.getName() << " unlocks resource '" << resource << "'";
    // Only act we are actually holding this resource
    if(getLockState(resource) == lock_state::LOCKED)
    {
        // Change internal state
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;

        // Update ID to have been executed
        mLockStates[resource].mToken.mLastRequestNumber[mSelf] = mLockStates[resource].mRequestNumber[mSelf];

        // Forward the token
        forwardToken(resource);
    } else{
        throw std::invalid_argument("SuzukiKasami::unlock: resource '" + resource + "' is not locked");
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

        LOG_DEBUG_S << "Pending request, forward token to " << agent.getName();
        sendToken(agent, resource);
    } else {
        LOG_DEBUG_S << "'" << mSelf.getName() << "' No pending requests";
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
            LOG_DEBUG_S << "Incoming Token Request";
            handleIncomingTokenRequest(message);
            return true;
        case ACLMessage::PROPAGATE:
            LOG_DEBUG_S << "Incoming Token";
            handleIncomingToken(message);
            return true;
        case ACLMessage::FAILURE:
            LOG_DEBUG_S << "Incoming Failure Message";
            handleIncomingFailure(message);
            return true;
        default:
            return false;
    }
}

void SuzukiKasami::handleIncomingTokenRequest(const fipa::acl::ACLMessage& message)
{
    // Extract information
    std::string resource;
    int sequenceNumber;
    extractInformation(message, resource, sequenceNumber);
    fipa::acl::AgentID agent = message.getSender();

    // Update our request state
    std::map<fipa::acl::AgentID, int>::const_iterator cit = mLockStates[resource].mRequestNumber.find(agent);
    if(cit != mLockStates[resource].mRequestNumber.end())
    {
        if(mLockStates[resource].mRequestNumber[agent] < sequenceNumber)
        {
            mLockStates[resource].mRequestNumber[agent] = sequenceNumber;
            mLockStates[resource].mConversationID[agent] = message.getConversationID();
        } else {
            LOG_INFO_S << "'" << mSelf.getName() << "' received an outdated token request from '" << agent.getName() << "'";
            return;
        }
    } else {
        LOG_DEBUG_S << "'" << mSelf.getName() << "' registering request of '" << agent.getName() << "' for resource '" << resource << "' with conversation id: " << message.getConversationID();
        mLockStates[resource].mRequestNumber[agent] = sequenceNumber;
        mLockStates[resource].mConversationID[agent] = message.getConversationID();
    }

    // If we hold the token && are not holding the lock && the sequenceNumber
    // indicates an outstanding request: we send the token.
    if(mLockStates[resource].mHoldingToken)
    {
        if(getLockState(resource) == lock_state::LOCKED)
        {
            LOG_DEBUG_S << "'" << mSelf.getName() << "' resource is locked";
        } else if(hasOutstandingRequest(resource, agent))
        {
            LOG_DEBUG_S << "'" << mSelf.getName() << "' agent '" << agent.getName() << "' has outstanding request";
            sendToken(agent, resource);
            return;
        } else {
            LOG_DEBUG_S << "'" << mSelf.getName() << "' agent '" << agent.getName() << "' has no outstanding request";
        }

        updateToken(resource, agent, sequenceNumber);

    } else {
        LOG_DEBUG_S << "'" << mSelf.getName() << "' not holding the token";
    }
}

bool SuzukiKasami::hasOutstandingRequest(const std::string& resource, const fipa::acl::AgentID& agent)
{
    int currentRequestNumber = mLockStates[resource].mRequestNumber[agent];
    int lastRequestNumber = mLockStates[resource].mToken.mLastRequestNumber[agent];

    LOG_DEBUG_S << "'" << mSelf.getName() << "' resource: '" << resource << "', agent: '" << agent.getName() << "', currentRequestNumber: " << currentRequestNumber << ", lastRequestNumber: " << lastRequestNumber;
    return currentRequestNumber == lastRequestNumber + 1;
}



void SuzukiKasami::updateToken(const std::string& resource, const fipa::acl::AgentID& requestor, int sequenceNumber)
{
    mLockStates[resource].mToken.mQueue.push_back(requestor);
    mLockStates[resource].mToken.mLastRequestNumber[requestor] = sequenceNumber;
}

void SuzukiKasami::handleIncomingToken(const fipa::acl::ACLMessage& message)
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
}

void SuzukiKasami::handleIncomingFailure(const fipa::acl::ACLMessage& message)
{
    // First determine the affected resource from the conversation id.
    std::string conversationID = message.getConversationID();
    std::string resource;

    std::map<std::string, ResourceLockState>::const_iterator it = mLockStates.begin();
    for(; it != mLockStates.end(); ++it)
    {
        const ResourceLockState& lockState = it->second;
        std::map<fipa::acl::AgentID, std::string>::const_iterator cit = lockState.mConversationID.begin();
        for(; cit != lockState.mConversationID.end(); ++cit)
        {
            if(cit->second == conversationID)
            {
                resource = it->first;
                break;
            }
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
        }
        // Otherwise somebody will foward the token to us at some point. (else block)
    }
    else
    {
        // The agent was not important (for us), we have to remove it from the list of communication partners, as we won't get a response from it
        mLockStates[resource].removeCommunicationPartner(intendedReceiver);
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

void SuzukiKasami::sendToken(const fipa::acl::AgentID& receiver, const std::string& resource)
{
    // We must unset holdingToken
    mLockStates[resource].mHoldingToken = false;

    using namespace fipa::acl;
    ACLMessage tokenMessage = prepareMessage(ACLMessage::PROPAGATE, getProtocolName());
    tokenMessage.addReceiver(receiver);
    std::string conversationID = mLockStates[resource].mConversationID[receiver];
    if(conversationID.empty())
    {
        LOG_INFO_S << "'" << mSelf.getName() + "' returning token to owner '" + receiver.getName() + "' -- though not requested";
        // continue in this conversation
        conversationID = mLockStates[resource].mConversationID[mSelf];
    } else {
        LOG_INFO_S << "'" << mSelf.getName() + "' returning token to owner '" + receiver.getName() + "' -- owner requested return";
    }

    tokenMessage.setConversationID(conversationID);

    // Our response messages are in the format "BOOST_ARCHIVE(RESOURCE_IDENTIFIER, TOKEN)"
    std::stringstream ss;
    // save data to archive
    // write class instance to archive
    boost::archive::text_oarchive oa(ss);
    oa << resource;
    Token token = mLockStates[resource].mToken;
    oa << token;
    tokenMessage.setContent(ss.str());
    tokenMessage.setLanguage(token.getTypeName());

    sendMessage(tokenMessage);
}

void SuzukiKasami::ResourceLockState::removeCommunicationPartner(const fipa::acl::AgentID& agent)
{
    mCommunicationPartners.erase(std::remove(mCommunicationPartners.begin(), mCommunicationPartners.end(), agent), mCommunicationPartners.end());
}

} // namespace distributed_locking
} // namespace fipa
