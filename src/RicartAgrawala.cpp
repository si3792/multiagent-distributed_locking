#include "RicartAgrawala.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <stdexcept>

namespace fipa {
namespace distributed_locking {

RicartAgrawala::RicartAgrawala() 
    : DLM()
{
}

RicartAgrawala::RicartAgrawala(const Agent& self, const std::vector<Agent>& agents) 
    : DLM(self, agents)
{
}

void RicartAgrawala::lock(const std::string& resource)
{
    // Only act we are not holding this resource and not already interested in it
    if(isLocked(resource) || mInterests.count(resource) != 0)
    {
        return;
    }

    using namespace fipa::acl;
    // Send a message to everyone, requesting the lock
    ACLMessage message;
    // A simple request
    message.setPerformative(ACLMessage::REQUEST);

    // FIXME: IDENTIFIER ???
    // Our request messages are in the format "TIME\nRESOURCE_IDENT"
    base::Time time = base::Time::now();
    message.setContent(time.toString() + "\n" + resource);
    // Add sender and receivers
    message.setSender(AgentID(mSelf.identifier));
    for(unsigned int i = 0; i < mAgents.size(); i++)
    {
        message.addReceiver(AgentID(mAgents[i].identifier));
    }

    // Set and increase conversation ID
    message.setConversationID(mSelf.identifier + boost::lexical_cast<std::string>(mConversationIDnum));
    mConversationIDnum++;
    
    // FIXME: use a mapping between enums and strings here --> see boost::assign::map_list_of and examples in fipa_acl
    // to prepare for adding one or more distributed allocation protocols
    // additionally that string / enum should be a static entry for this DLM subclass in order to identify it
    message.setProtocol("ricart_agrawala");

    // Add to outgoing messages and to interest list
    mOutgoingMessages.push_back(message);
    mInterests[resource] = time;

    // Now a response from each agent must be received before we can enter the critical section
}

void RicartAgrawala::unlock(const std::string& resource)
{
    // Only act we are actually holding this resource
    if(isLocked(resource))
    {
        // Remove from heldResources
        mHeldResources.remove(resource);
        // Send all deferred messages for that resource
        sendAllDeferredMessages(resource);
    }
}

bool RicartAgrawala::isLocked(const std::string& resource)
{
    return std::find(mHeldResources.begin(), mHeldResources.end(), resource) != mHeldResources.end();
}

void RicartAgrawala::onIncomingMessage(const fipa::acl::ACLMessage& message)
{
    using namespace fipa::acl;
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
    response.setProtocol("ricart_agrawala");

    // TODO what in the case of same time??
    // We send this message now, if we don't hold the resource and are not interested or have been slower. Otherwise we defer it.
    if(!isLocked(resource) &&
            (mInterests.count(resource) == 0 || otherTime < mInterests.at(resource)))
    {
        // Our response messages are in the format "TIME\nRESOURCE_IDENT"
        response.setContent(base::Time::now().toString() +"\n" + resource);
        mOutgoingMessages.push_back(response);
    }
    else
    {
        // We will have to add the timestamp later!
        response.setContent(resource);
        mDeferredMessages.push_back(response);
    }
}

void RicartAgrawala::handleIncomingResponse(const fipa::acl::ACLMessage& message)
{
    // If we get a response, that likely means, we are interested in a resource
    base::Time otherTime;
    std::string resource;
    extractInformation(message, otherTime, resource);

    // If we're not interested in that resource, we can ignore the message
    if(mInterests.count(resource) == 0)
    {
        return;
    }

    // Increase the number of responses for that resource
    mNumberOfResponses[resource]++;
    // We have got the lock, if all agents responded
    // XXX this does not work if agents have been added afterwards and got no request message
    if(mNumberOfResponses[resource] == mAgents.size())
    {
        mNumberOfResponses[resource] = 0;
        mInterests.erase(resource);
        mHeldResources.push_back(resource);
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
        throw std::runtime_error("DLM::extractInformation ACLMessage content malformed");
    }
    // Save the extracted information in the references
    time = base::Time::fromString(strs[0]);
    resource = strs[1];
}

void RicartAgrawala::sendAllDeferredMessages(const std::string& resource)
{
    for(std::list<fipa::acl::ACLMessage>::iterator it = mDeferredMessages.begin(); it != mDeferredMessages.end(); it++)
    {
        fipa::acl::ACLMessage msg = *it;
        if(msg.getContent() != resource)
        {
            // Ignore deferred messages for other resources
            continue;
        }
        // Include timestamp
        msg.setContent(base::Time::now().toString() +"\n" + msg.getContent());
        // Remove from deferredMessages, decrement it and add to outgoingMessages
        mDeferredMessages.erase(it);
        it--;
        mOutgoingMessages.push_back(msg);
    }
}

} // namespace distributed_locking
} // namespace fipa