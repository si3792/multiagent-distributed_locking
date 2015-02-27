#include "SuzukiKasamiExtended.hpp"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

SuzukiKasamiExtended::SuzukiKasamiExtended(const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
    : SuzukiKasami(self, resources)
{
    setProtocol(protocol::SUZUKI_KASAMI_EXTENDED);
}

void SuzukiKasamiExtended::forwardToken(const std::string& resource)
{

    if(mOwnedResources[resource] != mSelf.getName())
    {
        // If we're not the resource owner, we forward the token to him.
        sendToken(mOwnedResources[resource], resource);
    }
    else
    {
        // If we are the resource owner, we forward normally.
        SuzukiKasami::forwardToken(resource);
    }
}

bool SuzukiKasamiExtended::isTokenHolder(const std::string& resource, const fipa::acl::AgentID& agent)
{
    return mTokenHolders[resource] == agent;
}

void SuzukiKasamiExtended::sendToken(const acl::AgentID& receiver, const std::string& resource)
{
    fipa::distributed_locking::SuzukiKasami::sendToken(receiver, resource);
    // Additional actions only need to be taken, if we're the resource owner
    if(mOwnedResources[resource] == mSelf.getName())
    {
        // After sending the token, we must update mTokenHolders
        mTokenHolders[resource] = receiver.getName();
        // We must start sending PROBEs to the token owner
        startRequestingProbes(receiver.getName(), resource);
    }
}

void SuzukiKasamiExtended::handleIncomingToken(const acl::ACLMessage& message)
{
    // We need to extract the info twice now, this is kinda bad.
    std::string resource;
    Token token; // Just a dummy
    // This HAS to be done in two steps, as resource is not known before extractInformation returns!
    extractInformation(message, resource, token);

    // Before we could possibly forward the token again, we must (if we're the owner update mTokenHolders and)
    // stop sending PROBEs
    if(mOwnedResources[resource] == mSelf.getName())
    {
        // We own the token again.
        mTokenHolders[resource] = mSelf.getName();
    }
    // We must stop sending PROBEs to the former token owner
    stopRequestingProbes(message.getSender().getName(), resource);

    fipa::distributed_locking::SuzukiKasami::handleIncomingToken(message);
}

void SuzukiKasamiExtended::lock(const std::string& resource, const AgentIDList& agents)
{
    fipa::distributed_locking::SuzukiKasami::lock(resource, agents);
    // We start sending PROBEs to the resource owner
    startRequestingProbes(mOwnedResources[resource], resource);
}

} // namespace distributed_locking
} // namespace fipa
