#include "DLM.hpp"
#include "RicartAgrawala.hpp"
#include "RicartAgrawalaExtended.hpp"
#include "SuzukiKasami.hpp"
#include "SuzukiKasamiExtended.hpp"

#include <stdexcept>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <base/Logging.hpp>

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

// Initialize the Protocol->string mapping
std::map<protocol::Protocol, std::string> DLM::protocolTxt = boost::assign::map_list_of
    (protocol::DLM_DISCOVER, "dlm_discover")
    (protocol::DLM_PROBE, "dlm_probe")
    (protocol::RICART_AGRAWALA, "ricart_agrawala")
    (protocol::RICART_AGRAWALA_EXTENDED, "ricart_agrawala_extended")
    (protocol::SUZUKI_KASAMI, "suzuki_kasami")
    (protocol::SUZUKI_KASAMI_EXTENDED, "suzuki_kasami_extended");

// And other stuff
const int DLM::probeTimeoutSeconds = 5;


DLM::Ptr DLM::create(fipa::distributed_locking::protocol::Protocol implementation, const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
{
    switch(implementation)
    {
        case protocol::RICART_AGRAWALA:
            return DLM::Ptr( new RicartAgrawala(self, resources) );
        case protocol::RICART_AGRAWALA_EXTENDED:
            return DLM::Ptr( new RicartAgrawalaExtended(self, resources) );
        case protocol::SUZUKI_KASAMI:
            return DLM::Ptr( new SuzukiKasami(self, resources) );
        case protocol::SUZUKI_KASAMI_EXTENDED:
            return DLM::Ptr( new SuzukiKasamiExtended(self, resources) );
        default:
            throw std::invalid_argument("fipa::distributed_locking::DLM: unknown protocol requested");
    }
}

DLM::DLM(protocol::Protocol protocol, const fipa::acl::AgentID& self, const std::vector<std::string>& resources)
    : mSelf(self)
    , mProtocol(protocol)
    , mConversationIDnum(0)
    , mConversationMonitor(self)
{
    std::vector<std::string>::const_iterator cit = resources.begin();
    for(; cit != resources.end(); ++cit)
    {
        LOG_DEBUG_S << "Register: resource '" << *cit << "' with owner: '" << self.getName() << "'";
        mOwnedResources[*cit] = mSelf;
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

const fipa::acl::AgentID& DLM::getSelf() const
{
    return mSelf;
}

void DLM::setSelf(const fipa::acl::AgentID& self)
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

void DLM::lock(const std::string& resource, const AgentIDList& agents)
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

void DLM::agentFailed(const fipa::acl::AgentID& agent)
{
    throw std::runtime_error("DLM::agentFailed not implemented");
}

bool DLM::onIncomingMessage(const acl::ACLMessage& message)
{
    fipa::acl::ConversationPtr conversation = mConversationMonitor.getOrCreateConversation(message.getConversationID());
    conversation->update(message);

    // Debug:
    if(fipa::acl::ACLMessage::performativeFromString(message.getPerformative()) == fipa::acl::ACLMessage::FAILURE)
    {
        LOG_DEBUG_S << message.toString();
    }

    // Check if it's the right protocol
    std::string protocol = message.getProtocol();
    if(protocol != getProtocolName() && protocol != getProtocolTxt(protocol::DLM_PROBE) && protocol != getProtocolTxt(protocol::DLM_DISCOVER))
    {
        return false;
    }
    using namespace fipa::acl;
    // Abort if we're not a receiver
    fipa::acl::AgentIDList receivers = message.getAllReceivers();
    // Check if self is receiver of this message and return if not
    fipa::acl::AgentIDList::iterator cit = std::find(receivers.begin(), receivers.end(), mSelf);
    if(cit == receivers.end())
    {
        throw std::runtime_error("Message delivered which has not been addressed to this agent");
    }

    // Handle probe messages if necessary
    if(protocol == getProtocolTxt(protocol::DLM_PROBE))
    {
        return onIncomingProbeMessage(message);
    } else {
        return onIncomingDLMMessage(message);
    }
}

void DLM::discover(const std::string& resource, const AgentIDList& agents)
{
    if(hasKnownOwner(resource))
    {
        return;
    } else {
        mOwnedResources[resource] = AgentID();
        LOG_DEBUG_S << "'" << mSelf.getName() << "' query ownership information on '" << resource << "'";
        // Otherwise send a broadcast message to get that information
        using namespace fipa::acl;
        ACLMessage message = prepareMessage(ACLMessage::QUERY_IF, getProtocolTxt(protocol::DLM_DISCOVER));
        // Our requestOwnerInformation messages are in the format "RESOURCE_IDENTIFIER" --> content language
        message.setContent(resource);
        // Add receivers
        for(AgentIDList::const_iterator it = agents.begin(); it != agents.end(); it++)
        {
            message.addReceiver(*it);
        }

        // Add to outgoing messages
        sendMessage(message);
    }
}

bool DLM::onIncomingDLMMessage(const acl::ACLMessage& message)
{
    LOG_DEBUG_S << "'" << getSelf().getName() << "' Handling message: " << message.toString();
    using namespace fipa::acl;
    switch(message.getPerformativeAsEnum())
    {
        case ACLMessage::QUERY_IF:
        {
            std::string resource = message.getContent();
            // If we are the physical owner of that resource, we reply with that information. Otherwise, we ignore the message
            if(mOwnedResources[resource] == mSelf)
            {
                // By making the reply also a broadcast, we can save messages later, if other agents want to lock the same resource
                ACLMessage response = prepareMessage(ACLMessage::INFORM, getProtocolTxt(protocol::DLM_DISCOVER), resource);

                // Broadcasting to all receivers
                fipa::acl::AgentIDList receivers = message.getAllReceivers();
                // remove ourselves..
                for(AgentIDList::iterator it = receivers.begin(); it != receivers.end(); it++)
                {
                    if(*it == mSelf)
                    {
                        receivers.erase(it);
                        break;
                    }
                }
                // of course add the requestor to the receivers' list
                receivers.push_back(message.getSender());
                response.setAllReceivers(receivers);

                // Reply within the same conversation
                response.setConversationID(message.getConversationID());

                // Add to outgoing messages
                sendMessage(response);
            }
        }
        return true;
        case ACLMessage::INFORM:
            {
                // inform about ownership of this resource
                std::string resource = message.getContent();
                ResourceAgentMap::iterator it = mOwnedResources.find(resource);
                if(it != mOwnedResources.end())
                {
                    mOwnedResources[resource] = message.getSender();
                    LOG_DEBUG_S << "'" << mSelf.getName() << "' received owner information about '" << resource << "': " << message.getSender().getName();
                    return true;
                } else {
                    LOG_DEBUG_S << "'" << mSelf.getName() << "' ignoring inform message at this point, since it did not provide owner information, but '" << resource << "': " << message.getSender().getName() << " -- size: " << mOwnedResources.size();
                    return false;
                }

            }
        case ACLMessage::CONFIRM:
            {
                // confirmed that resource lock is held by the sender of
                // the received message
                std::string resource = message.getContent();
                mLockHolders[resource] = message.getSender();

                LOG_DEBUG_S << "'" << mSelf.getName() << "' received confirmation about lock on resource '" << resource << "' from " << message.getSender().getName();

                // Start sending PROBE messages to the owner
                startRequestingProbes(message.getSender(), resource);
            }
            return true;
        case ACLMessage::DISCONFIRM:
            {
                // disconfirm that the sender of this message still holds
                // the lock on the resource listed
                std::string resource = message.getContent();
                // Stop sending PROBE messages to the owner
                stopRequestingProbes(message.getSender(), resource);

                LOG_DEBUG_S << "'" << mSelf.getName() << "' received confirmation about release of lock on resource '" << resource << "' from " << message.getSender().getName();
                if(message.getSender() == mLockHolders[resource])
                {
                    // Only erase if the sender was the logical owner, as messages can come in wrong order
                    mLockHolders.erase(resource);
                }
            }
            return true;
        default:
            // Allow other performative to pass through for protocol extensions
            return false;
    }
}

bool DLM::onIncomingProbeMessage(const acl::ACLMessage& message)
{
    using namespace fipa::acl;
    if(message.getPerformativeAsEnum() == ACLMessage::REQUEST)
    {
        LOG_DEBUG_S << "'" << mSelf.getName() << "' received probe request from '" << message.getSender().getName();
        // Answer with CONFIRM:SUCCESS
        ACLMessage response = prepareMessage(ACLMessage::CONFIRM, getProtocolTxt(protocol::DLM_PROBE));
        response.addReceiver(message.getSender());
        // Respond within same conversation
        response.setConversationID(message.getConversationID());

        // Add to outgoing messages
        sendMessage(response);
    } else if(message.getPerformativeAsEnum() == ACLMessage::CONFIRM)
    {
        LOG_DEBUG_S << "'" << mSelf.getName() << "' received probe reply from '" << message.getSender().getName();
        // Set success in the ProbeRunner
        mProbeRunners[message.getSender()].mSuccess = true;
    }

    return true;
}

bool DLM::hasKnownOwner(const std::string& resource) const
{
    ResourceAgentMap::const_iterator cit = mOwnedResources.find(resource);
    // if we already know the physical owner of that resource, we don't have to do anything
    if(cit !=  mOwnedResources.end() && cit->second != fipa::acl::AgentID())
    {
        LOG_DEBUG_S << "Found owner: '" << cit->second.getName() << "' for resource '" << resource << "'";
        return true;
    }
    LOG_WARN_S << mSelf.getName() << " did not know the owner of '" << resource << "'";
    return false;
}

void DLM::lockObtained(const std::string& resource, const std::string& conversationId)
{
    LOG_DEBUG_S << "CONFIRM that '" << mSelf.getName() << " obtained lock for '" << resource << "'";
    if(mOwnedResources[resource] == mSelf)
    {
        // If this is our own resource, we can simply set us as the logical owner
        mLockHolders[resource] = mSelf;
    } else
    {
        if(!hasKnownOwner(resource))
        {
            // no to inform -- actually that should not happen
            std::runtime_error("DLM::lockObtained: lock obtained for resource '" + resource + "' but the actual owner of the resource is not known'");
            return;
        }

        using namespace fipa::acl;
        // confirm to owner that the lock has taken by this senders message
        ACLMessage message = prepareMessage(ACLMessage::CONFIRM, getProtocolName(), resource);
        message.setConversationID(conversationId);
        // send to owner
        message.addReceiver( mOwnedResources[resource] );
        // Add to outgoing messages
        sendMessage(message);
    }
}

fipa::acl::ACLMessage DLM::prepareMessage(fipa::acl::ACLMessage::Performative performative, const std::string& protocol, const std::string& content)
{
    ACLMessage message(performative);
    // Add sender
    message.setSender(mSelf);
    // The DLM protocol is not in the Protocol enum, as it is not a DLM implementation!
    message.setProtocol(protocol);
    message.setContent(content);
    // Set and increase conversation ID
    message.setConversationID(mSelf.getName() + "_" + boost::lexical_cast<std::string>(mConversationIDnum++));
    return message;
}

void DLM::lockReleased(const std::string& resource, const std::string& conversationId)
{
    if(mOwnedResources[resource] == mSelf)
    {
        // If this is our own resource, we can simply unset us as the logical owner
        // Only erase if we were the logical owner
        if(mLockHolders[resource] == mSelf)
        {
            mLockHolders.erase(resource);
        }
    }
    else
    {
        // Otherwise, if we know the physical owner, we have to send him an ACL message
        if(!hasKnownOwner(resource))
        {
            std::runtime_error("DLM::lockReleased: lock released for resource '" + resource + "' but the actual owner of the resource is not known'");
            return;
        }

        using namespace fipa::acl;
        ACLMessage message = prepareMessage(ACLMessage::DISCONFIRM, getProtocolName(), resource);
        message.addReceiver(mOwnedResources[resource]);
        message.setConversationID(conversationId);
        // Add to outgoing messages
        sendMessage(message);
    }
}

void DLM::startRequestingProbes(const fipa::acl::AgentID& agent, const std::string resourceName)
{
    LOG_DEBUG_S << "'" << mSelf.getName() << "' start probing '" << agent.getName() << " -- resource: " << resourceName;
    if(agent == mSelf)
    {
        throw std::invalid_argument("Agent '" + agent.getName() + "' trying to probe itself");
    }
    mProbeRunners[agent].mResources.push_back(resourceName);
}

void DLM::stopRequestingProbes(const fipa::acl::AgentID& agent, const std::string resourceName)
{
    LOG_DEBUG_S << "'" << mSelf.getName() << "' stop probing '" << agent.getName() << " -- resource: " << resourceName;
    ProbeRunnerMap::iterator it = mProbeRunners.find(agent);
    if(it != mProbeRunners.end())
    {
        ProbeRunner& runner = it->second;
        runner.mResources.remove(resourceName);
        // The following optimizes a little, but is not actually necessary
        if(runner.mResources.empty())
        {
            // delete this agents ProbeRunner
            mProbeRunners.erase(agent);
        }
    }
}

void DLM::trigger()
{
    // Loop through all ProbeRunners
    AgentIDList cleanupList;

    ProbeRunnerMap::iterator it = mProbeRunners.begin();
    for(; it != mProbeRunners.end();++it)
    {
        AgentID agent = it->first;
        ProbeRunner& runner = it->second;

        // Only act, if the resource list is not empty
        if(!runner.mResources.empty())
        {
            // If we already sent a probe message, and it is longer ago than the threshold,
            // we need to check for a response
            if(!runner.mTimeStamp.isNull() && base::Time::now() > runner.mTimeStamp + base::Time::fromSeconds(probeTimeoutSeconds))
            {
                if(runner.mSuccess)
                {
                    // Send another probe and update timestamp
                    runner.mTimeStamp = base::Time::now();
                    sendProbe(agent);
                    LOG_INFO_S << mSelf.getName() << " sent probe to " << agent.getName() << " after getting a success response.";
                } else
                {
                    // The agent failed. Stop annoying it then.
                    cleanupList.push_back(agent);
                    LOG_INFO_S << mSelf.getName() << " got no response from " << agent.getName();
                    // And call agentFailed
                    agentFailed(agent);
                    continue;
                }
            }
            // If we never sent a probe message, we better get going
            if(it->second.mTimeStamp.isNull())
            {
                it->second.mTimeStamp = base::Time::now();
                sendProbe(agent);
                LOG_DEBUG_S << mSelf.getName() << " sent probe to " << agent.getName() << " for the first time.";
            }
        }
    }

    AgentIDList::const_iterator cit = cleanupList.begin();
    for(; cit != cleanupList.end(); ++cit)
    {
        mProbeRunners.erase(*cit);
    }
}

void DLM::sendProbe(const fipa::acl::AgentID& agent)
{
    LOG_DEBUG_S << "'" << mSelf.getName() << "' sending probe to '" << agent.getName() << "'";
    // When sending a probe, success is false until we get a response
    mProbeRunners[agent].mSuccess = false;

    using namespace fipa::acl;
    ACLMessage message = prepareMessage(ACLMessage::REQUEST, getProtocolTxt(protocol::DLM_PROBE));
    message.addReceiver(agent);

    // Add to outgoing messages
    sendMessage(message);
}

protocol::Protocol DLM::getProtocol() const
{
    return mProtocol;
}

void DLM::sendMessage(const fipa::acl::ACLMessage& message)
{
    fipa::acl::ConversationPtr conversation = mConversationMonitor.getOrCreateConversation(message.getConversationID());
    conversation->update(message);
    mOutgoingMessages.push_back(message);
}

} // namespace distributed_locking
} // namespace fipa
