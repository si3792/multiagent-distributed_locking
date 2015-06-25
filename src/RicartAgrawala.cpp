#include "RicartAgrawala.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <stdexcept>
#include <base/Logging.hpp>

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

RicartAgrawala::RicartAgrawala(const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
    : DLM(protocol::RICART_AGRAWALA, self, resources)
{
}

void RicartAgrawala::lock(const std::string& resource, const AgentIDList& agents)
{
    if(!hasKnownOwner(resource))
    {
        throw std::invalid_argument("RicartAgrawala: cannot lock resource '" + resource + "' -- owner is unknown. Perform discovery first");
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

    using namespace fipa::acl;
    // Send a message to everyone, requesting the lock -- creates a  new
    // conversation
    ACLMessage message = prepareMessage(ACLMessage::REQUEST, getProtocolName());
    // Our request messages are in the format "TIME\nRESOURCE_IDENTIFIER"
    base::Time time = base::Time::now();
    message.setContent(time.toString() + "\n" + resource);
    // Add sender and receivers
    for(AgentIDList::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        message.addReceiver(*it);
    }

    // Add to outgoing messages
    sendMessage(message);

    // Change internal state
    mLockStates[resource].mCommunicationPartners = agents;
    mLockStates[resource].sort();
    mLockStates[resource].mResponded.clear();
    mLockStates[resource].mState = lock_state::INTERESTED;
    mLockStates[resource].mInterestTime = time;
    mLockStates[resource].mConversationID = message.getConversationID();
    // Now a response from each agent must be received before we can enter the critical section
    LOG_DEBUG_S << "'" << mSelf.getName() << "' mark INTERESTED for resource '" << resource << "'";
}

void RicartAgrawala::ResourceLockState::sort()
{
    // Sort agents
    std::sort(mCommunicationPartners.begin(), mCommunicationPartners.end());
    std::sort(mResponded.begin(), mResponded.end());
}

void RicartAgrawala::ResourceLockState::removeCommunicationPartner(const fipa::acl::AgentID& agent)
{
    mCommunicationPartners.erase(std::remove(mCommunicationPartners.begin(), mCommunicationPartners.end(), agent), mCommunicationPartners.end());
}


void RicartAgrawala::unlock(const std::string& resource)
{
    // Only act we are actually holding this resource
    if(getLockState(resource) == lock_state::LOCKED)
    {
        // Change internal state
        mLockStates[resource].mState = lock_state::NOT_INTERESTED;
        // Now a response from each agent must be received before we can enter the critical section
        LOG_DEBUG_S << "'" << mSelf.getName() << "' mark NOT_INTERESTED for resource '" << resource << "'";

        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);

        // Let the base class know we released the lock
        lockReleased(resource, mLockStates[resource].mConversationID);
    }
}

lock_state::LockState RicartAgrawala::getLockState(const std::string& resource) const
{
    std::map<std::string, ResourceLockState>::const_iterator cit = mLockStates.find(resource);
    if(cit != mLockStates.end())
    {
        return cit->second.mState;
    }
    else
    {
        // Otherwise return the default state
        return lock_state::NOT_INTERESTED;
    }
}

bool RicartAgrawala::onIncomingMessage(const fipa::acl::ACLMessage& message)
{
    LOG_DEBUG_S << "On incoming message: " << message.toString();
    // Call base method as required
    if( DLM::onIncomingMessage(message) )
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
        case ACLMessage::AGREE:
            handleIncomingResponse(message);
            return true;
        case ACLMessage::FAILURE:
            handleIncomingFailure(message);
            return true;
        default:
            // We ignore other performatives, as they are not part of our protocol.
            return false;
    }
}

void RicartAgrawala::handleIncomingRequest(const fipa::acl::ACLMessage& message)
{
    LOG_DEBUG_S << "Handling incoming request";
    base::Time otherTime;
    std::string resource;
    extractInformation(message, otherTime, resource);

    // Create a response
    fipa::acl::ACLMessage response = prepareMessage(ACLMessage::AGREE, getProtocolName());
    response.addReceiver(message.getSender());
    // Keep the conversation ID
    response.setConversationID(message.getConversationID());

    // We send this message now, if we don't hold the resource and are not interested or have been slower. Otherwise we defer it.
    lock_state::LockState state = getLockState(resource);
    if(state == lock_state::NOT_INTERESTED || (state == lock_state::INTERESTED && otherTime < mLockStates[resource].mInterestTime))
    {
        // Our response messages are in the format "TIME\nRESOURCE_IDENTIFIER"
        response.setContent(base::Time::now().toString() +"\n" + resource);
        sendMessage(response);
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
    LOG_DEBUG_S << "Handling incoming response";
    // If we get a response, that likely means, we are interested in a resource
    base::Time otherTime;
    std::string resource;
    extractInformation(message, otherTime, resource);

    // A response is only relevant if we're "INTERESTED"
    if(getLockState(resource) != lock_state::INTERESTED)
    {
        return;
    }

    // Save that the sender responded
    addRespondedAgent(message.getSender(), resource);

    // Check if we have enough responses, so that we don't sort and compare for each IncomingResponse
    if(mLockStates[resource].mCommunicationPartners.size() == mLockStates[resource].mResponded.size() )
    {
        // Sort agents who responded
        mLockStates[resource].sort();
        if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
        {
            mLockStates[resource].mState = lock_state::LOCKED;
            // Let the base class know we obtained the lock
            lockObtained(resource, message.getConversationID());
        }
        else
        {
          // This really shouldn't happen.
          throw std::runtime_error("RicartAgrawala::handleIncomingResponse received enough responses, but mCommunicationPartners not equal to mResponded");
        }
    }
}

void RicartAgrawala::addRespondedAgent(const fipa::acl::AgentID& agent, std::string resource)
{
    // XXX if agent becomes more complex, we need to copy it from mCommunicationPartners,
    // instead of creating a new one
    mLockStates[resource].mResponded.push_back(agent);
}


void RicartAgrawala::handleIncomingFailure(const fipa::acl::ACLMessage& message)
{
    LOG_DEBUG_S << "Handling incoming failure";
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
        LOG_DEBUG_S << "Ignore error since '" << mSelf.getName() << "' is not interested in resource: '" << resource << "'";
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

void RicartAgrawala::handleIncomingFailure(const std::string& resource, const fipa::acl::AgentID& intendedReceiver)
{
    // If the physical owner of the resource failed, the ressource probably cannot be obtained any more.
    if(mOwnedResources[resource] == intendedReceiver)
    {
        // Mark resource as unreachable.
        mLockStates[resource].mState = lock_state::UNREACHABLE;
        LOG_DEBUG_S << "'" << mSelf.getName()  << "' mark resource: '" << resource << "' unreachable";
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
    }
    else
    {
        // The agent was not important, we just have to remove it from the list of communication partners, as we won't get a response from it
        mLockStates[resource].removeCommunicationPartner(intendedReceiver);

        LOG_DEBUG_S << "'" << mSelf.getName()  << "' can ignore failed agent '" << intendedReceiver.getName()
            << "' since we never received a response regarding resource: '" << resource << "'";

        // We have got the lock, if all agents responded
        if(mLockStates[resource].mCommunicationPartners == mLockStates[resource].mResponded)
        {
            mLockStates[resource].mState = lock_state::LOCKED;

            // Let the base class know we obtained the lock
            lockObtained(resource, mLockStates[resource].mConversationID);
        }
    }
}

void RicartAgrawala::agentFailed(const fipa::acl::AgentID& agent)
{

    LOG_DEBUG_S << "'" << mSelf.getName() << "' detected failed agent: '" << agent.getName() << "'";
    std::map<std::string, ResourceLockState>::iterator it = mLockStates.begin();
    // Determine all resources, where we await an answer from that agent
    for(; it != mLockStates.end(); ++it)
    {
        ResourceLockState lockState = it->second;
        // If we're interested and await an answer from that agent...
        if(lockState.mState == lock_state::INTERESTED || lockState.mState == lock_state::LOCKED)
        {
            // If we're not interested or the agent already responded, we can ignore that
            LOG_DEBUG_S << "'" << mSelf.getName() << "' detect failed agent: " << agent.getName() << " which this agent holds a resource of or is interested in";

            if(std::find(lockState.mCommunicationPartners.begin(), lockState.mCommunicationPartners.end(), agent) != lockState.mCommunicationPartners.end())
            {
                LOG_DEBUG_S << "'" << mSelf.getName() << "' handle failed agent: '" << agent.getName() << "'";
                handleIncomingFailure(it->first, agent);
            } else {
                // If we're not interested or the agent already responded, we can ignore that
                LOG_DEBUG_S << "Agent failed: " << agent.getName() << " but this agent '" << mSelf.getName() << "' is not communcation partner without reponse";
            }
        } else {
                LOG_DEBUG_S << "'" << mSelf.getName() << "' is not interested in resource: '" << it->first << "' lock state is: " << lockState.mState;
        }
    }
}

void RicartAgrawala::extractInformation(const fipa::acl::ACLMessage& message, base::Time& time, std::string& resource)
{
    // Split by newline
    std::vector<std::string> strs;
    std::string s = message.getContent();
    boost::split(strs, s, boost::is_any_of("\n"));

    if(strs.size() != 2)
    {
        throw std::runtime_error("RicartAgrawala::extractInformation ACLMessage content malformed: " + s);
    }
    // Save the extracted information in the references
    time = base::Time::fromString(strs[0]);
    resource = strs[1];

    LOG_DEBUG_S << "Extracted time: " << time.toString() << " and resource: " << resource;
}

void RicartAgrawala::sendAllDeferredMessages(const std::string& resource)
{
    for(std::list<fipa::acl::ACLMessage>::iterator it = mLockStates[resource].mDeferredMessages.begin();
        it != mLockStates[resource].mDeferredMessages.end(); it++)
    {
        fipa::acl::ACLMessage msg = *it;
        LOG_DEBUG_S << "'" << mSelf.getName() << "' sent deferred message '" << msg.toString() << "'";
        // Include timestamp
        msg.setContent(base::Time::now().toString() +"\n" + msg.getContent());
        sendMessage(msg);
    }
    // Clear list
    mLockStates[resource].mDeferredMessages.clear();
}

} // namespace distributed_locking
} // namespace fipa
