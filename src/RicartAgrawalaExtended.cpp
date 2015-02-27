#include "RicartAgrawalaExtended.hpp"

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

RicartAgrawalaExtended::RicartAgrawalaExtended(const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
    : RicartAgrawala(self, resources)
{
    setProtocol(protocol::RICART_AGRAWALA_EXTENDED);
}

void RicartAgrawalaExtended::lock(const std::string& resource, const AgentIDList& agents)
{
    fipa::distributed_locking::RicartAgrawala::lock(resource, agents);
    // Start sending probes for all communication partners
    for(AgentIDList::const_iterator it = agents.begin(); it != agents.end(); ++it)
    {
        startRequestingProbes(*it, resource);
    }
}

void RicartAgrawalaExtended::addRespondedAgent(const AgentID& agentName, const std::string& resource)
{
    fipa::distributed_locking::RicartAgrawala::addRespondedAgent(agentName, resource);
    // Stop sending him probes
    stopRequestingProbes(agentName, resource);
}

} // namespace distributed_locking
} // namespace fipa
